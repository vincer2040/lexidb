#include "set.h"
#include "siphash.h"
#include "util.h"
#include <assert.h>
#include <memory.h>
#include <stdbool.h>
#include <stdlib.h>

#define SET_INITIAL_CAP 32

static set_entry* set_entry_new(void* data, size_t data_size);
static void set_entry_free(set_entry* entry, free_fn* fn);
static uint64_t set_hash(set* s, void* data, size_t data_size);

set set_new(size_t data_size, cmp_fn* key_cmp) {
    set s = {0};
    s.data_size = data_size;
    s.key_cmp = key_cmp;
    s.entries = calloc(SET_INITIAL_CAP, sizeof(set_entry*));
    assert(s.entries != NULL);
    s.capacity = SET_INITIAL_CAP;
    get_random_bytes(s.seed, SET_SEED_SIZE);
    return s;
}

size_t set_len(set* s) {
    return s->num_entries;
}

int set_insert(set* s, void* data, free_fn* fn) {
    uint64_t slot = set_hash(s, data, s->data_size);
    set_entry* cur = s->entries[slot];
    set_entry* new_entry;
    set_entry* head;
    if (!cur) {
        new_entry = set_entry_new(data, s->data_size);
        if (new_entry == NULL) {
            return -1;
        }
        s->entries[slot] = new_entry;
        s->num_entries++;
        return 0;
    }
    head = cur;
    while (cur) {
        int cmp;
        if (s->key_cmp) {
            cmp = s->key_cmp(data, cur->data);
        } else {
            cmp = memcmp(data, cur->data, s->data_size);
        }
        if (cmp == 0) {
            if (fn) {
                fn(data);
            }
            return 1;
        }
        cur = cur->next;
    }

    new_entry = set_entry_new(data, s->data_size);
    if (new_entry == NULL) {
        return -1;
    }

    new_entry->next = head;
    s->entries[slot] = new_entry;
    s->num_entries++;
    return 0;
}

bool set_has(set* s, void* data) {
    uint64_t slot = set_hash(s, data, s->data_size);
    set_entry* cur = s->entries[slot];
    if (!cur) {
        return false;
    }

    while (cur) {
        int cmp;
        if (s->key_cmp) {
            cmp = s->key_cmp(data, cur->data);
        } else {
            cmp = memcmp(data, cur->data, s->data_size);
        }
        if (cmp == 0) {
            return true;
        }
        cur = cur->next;
    }

    return false;
}

int set_delete(set* s, void* data, free_fn* fn) {
    uint64_t slot = set_hash(s, data, s->data_size);
    set_entry* cur = s->entries[slot];
    set_entry* prev = NULL;
    if (!cur) {
        return -1;
    }
    while (cur) {
        int cmp;
        if (s->key_cmp) {
            cmp = s->key_cmp(data, cur->data);
        } else {
            cmp = memcmp(data, cur->data, s->data_size);
        }

        if (cmp == 0) {
            if (!prev) {
                s->entries[slot] = cur->next;
            } else {
                prev->next = cur->next;
            }

            set_entry_free(cur, fn);
            s->num_entries--;
            return 0;
        }
        prev = cur;
        cur = cur->next;
    }

    return -1;
}

void set_free(set* s, free_fn* fn) {
    size_t i, len = s->capacity;
    for (i = 0; i < len; ++i) {
        set_entry* cur = s->entries[i];
        if (!cur) {
            continue;
        }
        while (cur) {
            set_entry* next = cur->next;
            set_entry_free(cur, fn);
            cur = next;
        }
    }
}

static uint64_t set_hash(set* s, void* data, size_t data_size) {
    return siphash(data, data_size, s->seed) % s->capacity;
}

static set_entry* set_entry_new(void* data, size_t data_size) {
    set_entry* entry;
    size_t needed = (sizeof *entry) + data_size;
    entry = malloc(needed);
    if (entry == NULL) {
        return NULL;
    }
    memset(entry, 0, needed);
    memcpy(entry->data, data, data_size);
    return entry;
}

static void set_entry_free(set_entry* entry, free_fn* fn) {
    if (fn) {
        fn(entry->data);
    }
    free(entry);
}
