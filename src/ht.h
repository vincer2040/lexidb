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
int ht_insert(Ht* ht, uint8_t* key, size_t key_len, void* value,
              size_t val_size, FreeCallBack* cb);
int ht_delete(Ht* ht, uint8_t* key, size_t key_len);
void* ht_get(Ht* ht, uint8_t* key, size_t key_len);
void ht_print(Ht* ht);
void ht_free(Ht* ht);

#endif
