#ifndef __HT_H__

#define __HT_H__

#include "util.h"
#include <stddef.h>

#define HT_SEED_SIZE 16

typedef enum {
    HT_OK,
    HT_INV_KEY,
    HT_OOM,
} ht_result;

typedef struct ht_entry {
    size_t key_size;
    struct ht_entry* next;
    unsigned char data[];
} ht_entry;

typedef struct {
    size_t num_entries;
    size_t capacity;
    size_t data_size;
    cmp_fn* key_cmp;
    unsigned char seed[HT_SEED_SIZE];
    ht_entry** entries;
} ht;

typedef struct {
    ht_entry* cur;
    ht_entry* next;
    ht* ht;
    size_t next_slot;
    size_t end_slot;
} ht_iter;

ht ht_new(size_t data_size, cmp_fn* key_cmp);
ht_result ht_insert(ht* ht, void* key, size_t key_size, void* data, free_fn* free_key,
              free_fn* free_data);
ht_result ht_try_insert(ht* ht, void* key, size_t key_size, void* data);
void* ht_get(ht* ht, void* key, size_t key_size);
ht_result ht_delete(ht* ht, void* key, size_t key_size, free_fn* free_key,
              free_fn* free_data);
const void* ht_entry_get_key(ht_entry* e);
const void* ht_entry_get_value(ht_entry* e);
void ht_free(ht* ht, free_fn* free_key, free_fn* free_data);

ht_iter ht_iter_new(ht* ht);
void ht_iter_next(ht_iter* iter);

#endif /* __HT_H__ */
