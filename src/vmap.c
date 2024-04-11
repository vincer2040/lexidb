#include "vmap.h"
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct vmap {
    vmap_type* type;
    uint64_t numel;
    uint64_t numel_and_deleted;
    uint64_t power;
    size_t entry_size;
    size_t entry_padding;
    uint8_t* metadata;
    unsigned char* entries;
    unsigned char data[];
};

#define VMAP_INITIAL_POWER 5

#define VMAP_MAX_LOAD .7
#define VMAP_MIN_LOAD .3

#define VMAP_DATA_OFFSET (uintptr_t)(&((vmap*)NULL)->data)

#define vmap_padding(size, hdr)                                                \
    ((sizeof(void*) - ((size + hdr) % sizeof(void*))) & (sizeof(void*) - 1))

#define VMAP_EMPTY (0)
#define VMAP_DELETED (1 << 0)
#define VMAP_FULL (1 << 1)
#define VMAP_MASK 3
#define VMAP_H2_MASK 255

#define vmap_free_key(map, key)                                                \
    do {                                                                       \
        if ((map)->type->key_free) {                                           \
            (map)->type->key_free((key));                                      \
        }                                                                      \
    } while (0)

#define vmap_free_value(map, value)                                            \
    do {                                                                       \
        if ((map)->type->value_free) {                                         \
            (map)->type->value_free((value));                                  \
        }                                                                      \
    } while (0)

#define vmap_key_cmp(map, a, b)                                                \
    (map)->type->key_cmp ? (map)->type->key_cmp((a), (b))                      \
                         : memcmp((a), (b), (map)->type->key_size)

#define vmap_h1(hash) ((hash) >> 8)
#define vmap_h2_from_hash(hash) ((hash & VMAP_H2_MASK) >> 2)
#define vmap_h2(byte) ((byte) >> 2)
#define vmap_flags(byte) (byte & VMAP_MASK)

static int vmap_resize(vmap** map, uint64_t new_power);
static vmap* vmap_new_with_capacity(vmap_type* type, uint64_t power,
                                    size_t entry_size, size_t entry_padding);

vmap* vmap_new(vmap_type* type) {
    vmap* map;
    size_t needed, entry_padding, entry_size, metadata_size, metadata_padding;
    uint64_t initial_cap = 1 << VMAP_INITIAL_POWER;
    entry_padding = vmap_padding(type->key_size, 0);
    entry_size = type->key_size + type->value_size + entry_padding;
    metadata_size = 1 << VMAP_INITIAL_POWER;
    metadata_padding = vmap_padding(metadata_size, VMAP_DATA_OFFSET);
    needed = (sizeof *map) + metadata_size + metadata_padding +
             (entry_size * initial_cap);
    map = malloc(needed);
    if (map == NULL) {
        return NULL;
    }
    memset(map, 0, needed);
    map->type = type;
    map->entry_size = entry_size;
    map->entry_padding = entry_padding;
    map->power = VMAP_INITIAL_POWER;
    map->metadata = map->data;
    map->entries = map->data + metadata_size + metadata_padding;
    return map;
}

int vmap_insert(vmap** map, void* key, void* value) {
    vmap* m = *map;
    uint64_t hash = m->type->hash(key);
    uint64_t h1 = vmap_h1(hash);
    uint8_t h2 = vmap_h2_from_hash(hash);
    uint64_t capacity = (1 << m->power);
    uint64_t slot = h1 & (capacity - 1);
    size_t key_size = m->type->key_size;
    size_t value_size = m->type->value_size;
    size_t offset = key_size + m->entry_padding;
    size_t entry_size = m->entry_size;
    double load;
    while (1) {
        uint8_t data = m->metadata[slot];
        uint8_t data_h2 = vmap_h2(data);
        uint8_t flags = vmap_flags(data);
        unsigned char* entry;
        if (flags == VMAP_DELETED) {
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        if (flags == VMAP_FULL) {
            entry = m->entries + (entry_size * slot);
            if (h2 == data_h2) {
                int cmp = vmap_key_cmp(m, entry, key);
                if (cmp == 0) {
                    vmap_free_key(m, key);
                    vmap_free_value(m, entry + offset);
                    memcpy(entry + offset, value, value_size);
                    break;
                }
            }
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        entry = m->entries + (entry_size * slot);
        memcpy(entry, key, key_size);
        memcpy(entry + offset, value, value_size);
        m->metadata[slot] = (h2 << 2) | VMAP_FULL;
        m->numel++;
        m->numel_and_deleted++;
        load = (double)m->numel_and_deleted / (double)capacity;
        if (load > VMAP_MAX_LOAD) {
            return vmap_resize(map, m->power + 1);
        }
        break;
    }
    return VMAP_OK;
}

const void* vmap_find(const vmap* map, const void* key) {
    uint64_t hash = map->type->hash(key);
    uint64_t h1 = vmap_h1(hash);
    uint8_t h2 = vmap_h2_from_hash(hash);
    uint64_t capacity = (1 << map->power);
    uint64_t slot = h1 & (capacity - 1);
    size_t key_size = map->type->key_size;
    size_t offset = key_size + map->entry_padding;
    size_t entry_size = map->entry_size;
    while (1) {
        uint8_t data = map->metadata[slot];
        uint8_t data_h2 = vmap_h2(data);
        uint8_t flags = vmap_flags(data);
        unsigned char* entry;
        if (flags == VMAP_DELETED) {
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        if (flags == VMAP_FULL) {
            entry = map->entries + (entry_size * slot);
            if (h2 == data_h2) {
                int cmp = vmap_key_cmp(map, entry, key);
                if (cmp == 0) {
                    return entry + offset;
                }
            }
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        break;
    }
    return NULL;
}

int vmap_delete(vmap** map, const void* key) {
    vmap* m = *map;
    uint64_t hash = m->type->hash(key);
    uint64_t h1 = vmap_h1(hash);
    uint8_t h2 = vmap_h2_from_hash(hash);
    uint64_t capacity = (1 << m->power);
    uint64_t slot = h1 & (capacity - 1);
    size_t key_size = m->type->key_size;
    size_t offset = key_size + m->entry_padding;
    size_t entry_size = m->entry_size;
    double load;
    while (1) {
        uint8_t data = m->metadata[slot];
        uint8_t data_h2 = vmap_h2(data);
        uint8_t flags = vmap_flags(data);
        unsigned char* entry;
        if (flags == VMAP_DELETED) {
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        if (flags == VMAP_FULL) {
            entry = m->entries + (entry_size * slot);
            if (h2 == data_h2) {
                int cmp = vmap_key_cmp(m, entry, key);
                if (cmp == 0) {
                    vmap_free_value(m, entry + offset);
                    vmap_free_key(m, entry);
                    m->metadata[slot] = VMAP_DELETED;
                    m->numel--;
                    load = (double)m->numel / (double)capacity;
                    if ((load < VMAP_MIN_LOAD) && (m->power > VMAP_INITIAL_POWER)) {
                        return vmap_resize(map, m->power - 1);
                    }
                    return VMAP_OK;
                }
            }
            slot = (slot + 1) & (capacity - 1);
            continue;
        }
        break;
    }
    return VMAP_NO_KEY;
}

void vmap_free(vmap* map) {
    uint64_t i, len = (1 << map->power);
    size_t key_size = map->type->key_size, entry_padding = map->entry_padding,
           entry_size = map->entry_size;
    for (i = 0; i < len; ++i) {
        uint8_t data = map->metadata[i];
        uint8_t flags = vmap_flags(data);
        unsigned char* entry;
        if (flags == VMAP_EMPTY) {
            continue;
        }
        if (flags == VMAP_DELETED) {
            continue;
        }
        entry = map->entries + (i * entry_size);
        vmap_free_value(map, entry + key_size + entry_padding);
        vmap_free_key(map, entry);
    }
    free(map);
}

static int vmap_resize(vmap** map, uint64_t new_power) {
    vmap* m = *map;
    size_t entry_size = m->entry_size, entry_padding = m->entry_padding;
    vmap* new_map =
        vmap_new_with_capacity(m->type, new_power, entry_size, entry_padding);
    uint64_t i, len = 1 << m->power, new_cap = 1 << new_power;
    uint64_t (*hash)(const void*) = m->type->hash;
    if (new_map == NULL) {
        return -1;
    }
    for (i = 0; i < len; ++i) {
        uint64_t new_hash, slot, h1;
        uint8_t data = m->metadata[i];
        uint8_t flags = vmap_flags(data);
        uint8_t h2;
        unsigned char* entry;
        if (flags == VMAP_EMPTY) {
            continue;
        }
        if (flags == VMAP_DELETED) {
            continue;
        }
        entry = m->entries + (entry_size * i);
        new_hash = hash(entry);
        h1 = vmap_h1(new_hash);
        h2 = vmap_h2_from_hash(new_hash);
        slot = h1 & (new_cap - 1);
        while (1) {
            uint8_t new_data = new_map->metadata[slot];
            uint8_t new_flags = vmap_flags(new_data);
            if (new_flags == VMAP_EMPTY) {
                memcpy(new_map->entries + (entry_size * slot), entry, entry_size);
                new_map->metadata[slot] = (h2 << 2) | VMAP_FULL;
                break;
            }
            slot = (slot + 1) & (new_cap - 1);
        }
    }
    new_map->numel = new_map->numel_and_deleted = m->numel;
    *map = new_map;
    free(m);
    return 0;
}

static vmap* vmap_new_with_capacity(vmap_type* type, uint64_t power,
                                    size_t entry_size, size_t entry_padding) {
    vmap* map;
    uint64_t cap = 1 << power;
    size_t metadata_size = 1 << power;
    size_t metadata_padding = vmap_padding(metadata_size, VMAP_DATA_OFFSET);
    size_t needed =
        (sizeof *map) + metadata_size + metadata_padding + (entry_size * cap);
    map = malloc(needed);
    if (map == NULL) {
        return NULL;
    }
    memset(map, 0, needed);
    map->type = type;
    map->power = power;
    map->metadata = map->data;
    map->entries = map->data + metadata_size + metadata_padding;
    map->entry_size = entry_size;
    map->entry_padding = entry_padding;
    return map;
}
