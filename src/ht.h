#ifndef __HT_H__

#define __HT_H__

#include "util.h"
#include <stddef.h>

#define HT_SEED_SIZE 16

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

ht ht_new(size_t data_size, cmp_fn* key_cmp);
int ht_insert(ht* ht, void* key, size_t key_size, void* data,
              free_fn* free_key, free_fn* free_data);
void* ht_get(ht* ht, void* key, size_t key_size);
int ht_delete(ht* ht, void* key, size_t key_size, free_fn* free_key,
              free_fn* free_data);
void ht_free(ht* ht, free_fn* free_key, free_fn* free_data);

#endif /* __HT_H__ */
