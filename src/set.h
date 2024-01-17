#ifndef __SET_H__

#define __SET_H__

#include "util.h"
#include <stdbool.h>
#include <stddef.h>

#define SET_SEED_SIZE 16

typedef enum {
    SET_OK,
    SET_KEY_EXISTS,
    SET_INV_KEY,
    SET_OOM,
} set_result;

typedef struct set_entry {
    struct set_entry* next;
    unsigned char data[];
} set_entry;

typedef struct {
    size_t num_entries;
    size_t capacity;
    size_t data_size;
    cmp_fn* key_cmp;
    unsigned char seed[SET_SEED_SIZE];
    set_entry** entries;
} set;

set set_new(size_t data_size, cmp_fn* key_cmp);
size_t set_len(set* s);
set_result set_insert(set* s, void* data, free_fn* fn);
bool set_has(set* s, void* data);
set_result set_delete(set* s, void* data, free_fn* fn);
void set_free(set* s, free_fn* fn);

#endif /* __SET_H__ */
