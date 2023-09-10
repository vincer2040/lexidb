#ifndef __HT_H__

#define __HT_H__

#include <stddef.h>
#include <stdint.h>

#define BUCKET_INITIAL_CAP 32

typedef void FreeCallBack(void* ptr);

typedef struct {
    size_t key_len;
    size_t val_size;
    uint8_t* key;
    void* value;
    FreeCallBack* cb;
} Entry;

typedef struct {
    size_t len;
    size_t cap;
    Entry* entries;
} Bucket;

typedef struct {
    size_t len;
    size_t cap;
    uint8_t seed[16];
    Bucket* buckets;
} Ht;

Ht* ht_new(size_t initial_cap);
int ht_try_insert(Ht* ht, uint8_t* key, size_t key_len, void* value,
                  size_t val_size, FreeCallBack* cb);
int ht_insert(Ht* ht, uint8_t* key, size_t key_len, void* value,
              size_t val_size, FreeCallBack* cb);
uint8_t ht_has(Ht* ht, uint8_t* key, size_t key_len);
int ht_delete(Ht* ht, uint8_t* key, size_t key_len);
void* ht_get(Ht* ht, uint8_t* key, size_t key_len);
size_t ht_len(Ht* ht);
void entry_print(Entry* e);
void ht_print(Ht* ht);
void ht_free(Ht* ht);

typedef struct {
    uint8_t* cur;
    uint8_t* next;
    size_t bucket_idx;
    size_t ht_idx;
    Ht* ht;
}HtKeysIter;

HtKeysIter* ht_keys_iter(Ht* ht);
void ht_keys_next(HtKeysIter* iter);
void ht_keys_iter_free(HtKeysIter* iter);

typedef struct {
    void* cur;
    void* next;
    size_t bucket_idx;
    size_t ht_idx;
    Ht* ht;
} HtValuesIter;

HtValuesIter* ht_values_iter(Ht* ht);
void ht_values_next(HtValuesIter* iter);
void ht_values_iter_free(HtValuesIter* iter);

typedef struct {
    Entry* cur;
    Entry* next;
    size_t bucket_idx;
    size_t ht_idx;
    Ht* ht;
} HtEntriesIter;

HtEntriesIter* ht_entries_iter(Ht* ht);
void ht_entries_next(HtEntriesIter* iter);
void ht_entries_iter_free(HtEntriesIter* iter);

#endif
