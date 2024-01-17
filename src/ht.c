#include "ht.h"
#include "siphash.h"
#include "util.h"
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#define HT_INITIAL_CAP 32

#define ht_padding(size)                                                       \
    ((sizeof(void*) - ((size + 16) % sizeof(void*))) & (sizeof(void*) - 1))

static uint64_t ht_hash(ht* ht, void* key, size_t key_size);
static ht_entry* ht_entry_new(void* key, size_t key_size, void* data,
                              size_t data_size);
static void ht_entry_free(ht_entry* e, free_fn* free_key, free_fn* free_data);
static ht_result ht_resize(ht* ht);

ht ht_new(size_t data_size, cmp_fn* key_cmp) {
    ht ht = {0};
    ht.data_size = data_size;
    ht.key_cmp = key_cmp;
    ht.entries = calloc(HT_INITIAL_CAP, sizeof(ht_entry*));
    assert(ht.entries != NULL);
    ht.capacity = HT_INITIAL_CAP;
    get_random_bytes(ht.seed, HT_SEED_SIZE);
    return ht;
}

ht_result ht_insert(ht* ht, void* key, size_t key_size, void* data, free_fn* free_key,
              free_fn* free_data) {
    uint64_t slot;
    ht_entry* cur;
    ht_entry* head;
    ht_entry* new_entry;

    if (ht->num_entries == ht->capacity) {
        ht_result resize = ht_resize(ht);
        if (resize != HT_OK) {
            return resize;
        }
    }

    slot = ht_hash(ht, key, key_size);
    cur = ht->entries[slot];

    if (cur == NULL) {
        cur = ht_entry_new(key, key_size, data, ht->data_size);
        if (cur == NULL) {
            return HT_OOM;
        }
        ht->entries[slot] = cur;
        ht->num_entries++;
        return HT_OK;
    }

    head = cur;

    while (cur) {
        int cmp;
        if (ht->key_cmp) {
            cmp = ht->key_cmp(key, cur->data);
        } else {
            if (key_size != cur->key_size) {
                cmp = 1;
            } else {
                cmp = memcmp(key, cur->data, key_size);
            }
        }

        if (cmp == 0) {
            size_t offset = key_size + ht_padding(key_size);
            if (free_data) {
                free_data(cur->data + offset);
            }
            if (free_key) {
                free_key(key);
            }
            memcpy(cur->data + offset, data, ht->data_size);
            return HT_OK;
        }
        cur = cur->next;
    }

    new_entry = ht_entry_new(key, key_size, data, ht->data_size);
    if (new_entry == NULL) {
        return HT_OOM;
    }

    new_entry->next = head;

    ht->entries[slot] = new_entry;

    ht->num_entries++;
    return HT_OK;
}

ht_result ht_try_insert(ht* ht, void* key, size_t key_size, void* data) {
    uint64_t slot;
    ht_entry* cur;
    ht_entry* head;
    ht_entry* new_entry;

    if (ht->num_entries == ht->capacity) {
        int resize = ht_resize(ht);
        if (resize != HT_OK) {
            return resize;
        }
    }

    slot = ht_hash(ht, key, key_size);
    cur = ht->entries[slot];
    if (cur == NULL) {
        new_entry = ht_entry_new(key, key_size, data, ht->data_size);
        if (new_entry == NULL) {
            return HT_OOM;
        }
        ht->entries[slot] = new_entry;
        ht->num_entries++;
        return 0;
    }

    head = cur;

    while (cur) {
        int cmp;
        if (ht->key_cmp) {
            cmp = ht->key_cmp(key, cur->data);
        } else {
            if (key_size != cur->key_size) {
                cmp = 1;
            } else {
                cmp = memcmp(key, cur->data, key_size);
            }
        }

        if (cmp == 0) {
            return HT_INV_KEY;
        }

        cur = cur->next;
    }

    new_entry = ht_entry_new(key, key_size, data, ht->data_size);
    if (new_entry == NULL) {
        return HT_OOM;
    }

    new_entry->next = head;

    ht->entries[slot] = new_entry;
    ht->num_entries++;
    return HT_OK;
}

void* ht_get(ht* ht, void* key, size_t key_size) {
    uint64_t slot = ht_hash(ht, key, key_size);
    ht_entry* cur = ht->entries[slot];
    if (!cur) {
        return NULL;
    }

    while (cur) {
        int cmp;
        if (ht->key_cmp) {
            cmp = ht->key_cmp(key, cur->data);
        } else {
            if (key_size != cur->key_size) {
                cmp = 1;
            } else {
                cmp = memcmp(key, cur->data, key_size);
            }
        }

        if (cmp == 0) {
            size_t offset = key_size + ht_padding(key_size);
            return cur->data + offset;
        }
        cur = cur->next;
    }
    return NULL;
}

ht_result ht_delete(ht* ht, void* key, size_t key_size, free_fn* free_key,
              free_fn* free_data) {
    uint64_t slot = ht_hash(ht, key, key_size);
    ht_entry* cur = ht->entries[slot];
    ht_entry* prev = NULL;
    if (!cur) {
        return HT_INV_KEY;
    }

    while (cur) {
        int cmp;
        if (ht->key_cmp) {
            cmp = ht->key_cmp(key, cur->data);
        } else {
            if (key_size != cur->key_size) {
                cmp = 1;
            } else {
                cmp = memcmp(key, cur->data, key_size);
            }
        }

        if (cmp == 0) {
            if (!prev) {
                ht->entries[slot] = cur->next;
            } else {
                prev->next = cur->next;
            }

            ht_entry_free(cur, free_key, free_data);
            ht->num_entries--;
            return HT_OK;
        }

        prev = cur;
        cur = cur->next;
    }
    return HT_INV_KEY;
}

const void* ht_entry_get_key(ht_entry* e) { return e->data; }

const void* ht_entry_get_value(ht_entry* e) {
    size_t offset = e->key_size + ht_padding(e->key_size);
    return e->data + offset;
}

void ht_free(ht* ht, free_fn* free_key, free_fn* free_data) {
    size_t i, len = ht->capacity;
    for (i = 0; i < len; ++i) {
        ht_entry* cur = ht->entries[i];
        if (!cur) {
            continue;
        }

        while (cur) {
            ht_entry* next = cur->next;
            ht_entry_free(cur, free_key, free_data);
            cur = next;
        }
    }
    free(ht->entries);
}

static uint64_t ht_hash(ht* ht, void* key, size_t key_size) {
    return siphash(key, key_size, ht->seed) % ht->capacity;
}

static ht_result ht_resize(ht* ht) {
    size_t i, len = ht->capacity;
    size_t new_cap = ht->capacity << 1;
    ht_entry** new_entries = calloc(new_cap, sizeof(ht_entry*));
    if (new_entries == NULL) {
        return HT_OOM;
    }
    ht->capacity = new_cap;
    for (i = 0; i < len; ++i) {
        ht_entry* cur = ht->entries[i];
        ht_entry* next;
        if (!cur) {
            continue;
        }
        while (cur) {
            uint64_t slot = ht_hash(ht, cur->data, cur->key_size);
            ht_entry* new_cur = new_entries[slot];
            next = cur->next;
            cur->next = NULL;

            if (!new_cur) {
                new_entries[slot] = cur;
            } else {
                cur->next = new_cur;
                new_entries[slot] = cur;
            }

            cur = next;
        }
    }

    free(ht->entries);
    ht->entries = new_entries;
    return HT_OK;
}

static ht_entry* ht_entry_new(void* key, size_t key_size, void* data,
                              size_t data_size) {
    ht_entry* e;
    size_t needed;
    size_t offset;
    if (data) {
        offset = key_size + ht_padding(key_size);
        needed = sizeof *e + offset + data_size;
    } else {
        needed = sizeof *e + key_size;
    }

    e = malloc(needed);
    memset(e, 0, needed);

    memcpy(e->data, key, key_size);

    if (data) {
        memcpy(e->data + offset, data, data_size);
    }

    e->key_size = key_size;
    return e;
}

static void ht_entry_free(ht_entry* e, free_fn* free_key, free_fn* free_data) {
    if (free_data) {
        size_t offset = e->key_size + ht_padding(e->key_size);
        free_data(e->data + offset);
    }
    if (free_key) {
        free_key(e->data);
    }
    free(e);
}

ht_iter ht_iter_new(ht* ht) {
    ht_iter iter = {0};
    iter.ht = ht;
    iter.end_slot = ht->capacity;
    ht_iter_next(&iter);
    ht_iter_next(&iter);
    return iter;
}

void ht_iter_next(ht_iter* iter) {
    size_t i, len = iter->end_slot;
    iter->cur = iter->next;
    if (iter->next_slot == iter->end_slot) {
        iter->next = NULL;
        return;
    }
    if (iter->next_slot == 0 && iter->next == NULL) {
        for (i = 0; i < len; ++i) {
            ht_entry* cur = iter->ht->entries[i];
            if (!cur) {
                continue;
            }
            iter->next = cur;
            iter->next_slot = i;
            return;
        }
        iter->next = NULL;
        iter->next_slot = len;
    }
    if (iter->next->next) {
        iter->next = iter->next->next;
        return;
    }

    for (i = iter->next_slot + 1; i < len; ++i) {
        ht_entry* cur = iter->ht->entries[i];
        if (!cur) {
            continue;
        }
        iter->next = cur;
        iter->next_slot = i;
        return;
    }
    iter->next = NULL;
    iter->next_slot = iter->end_slot;
}
