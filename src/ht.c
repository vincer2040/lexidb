#include "ht.h"
#include "sha256.h"
#include "siphash.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

static void free_entry(Entry* e);

typedef struct HtHasResult {
    uint8_t has;
    uint64_t hash;
    size_t insert;
} HtHasResult;

/* create a new hashtable */
Ht* ht_new(size_t initial_cap) {
    Ht* ht;

    assert(initial_cap > 0);

    ht = calloc(1, sizeof *ht);
    if (ht == NULL) {
        return NULL;
    }

    ht->buckets = calloc(initial_cap, sizeof(Bucket));
    if (ht->buckets == NULL) {
        free(ht);
        return NULL;
    }

    ht->cap = initial_cap;

    get_random_bytes(ht->seed, sizeof ht->seed);

    return ht;
}

inline uint64_t ht_hash(Ht* ht, uint8_t* key, size_t key_len) {
    return siphash(key, key_len, ht->seed) % ht->cap;
}

/* internal check to see if key and value are stored */
static void ht_internal_has(Ht* ht, uint8_t* key, size_t key_len,
                            HtHasResult* has) {
    uint64_t hash;
    Bucket bucket;
    size_t i, len;

    hash = ht_hash(ht, key, key_len);
    has->hash = hash;
    bucket = ht->buckets[hash];
    len = bucket.len;

    /* check if bucket has entries */
    if (len == 0) {
        has->has = 0;
        has->insert = 0;
        return;
    }

    for (i = 0; i < len; ++i) {
        Entry e;
        uint8_t* at_key;
        size_t at_len;

        e = bucket.entries[i];
        at_len = e.key_len;
        at_key = e.key;

        if (at_len != key_len)
            continue;
        if (memcmp(key, at_key, key_len) != 0)
            continue;

        /* found key */
        has->insert = i;
        has->has = 1;
        break;
    }
}

/* insert key and value into bucket */
static int bucket_insert(Bucket* bucket, uint8_t* key, size_t key_len,
                         void* value, size_t val_size, FreeCallBack* cb) {
    size_t len, cap;

    len = bucket->len;
    cap = bucket->cap;

    /* check if the bucket has been initialized */
    if (cap == 0) {
        bucket->entries = calloc(BUCKET_INITIAL_CAP, sizeof(Entry));
        cap = BUCKET_INITIAL_CAP;
        bucket->cap = cap;
    }

    /* check if bucket is full */
    if (len == cap) {
        cap += len;
        bucket->entries = realloc(bucket->entries, sizeof(Entry) * cap);
        if (bucket->entries == NULL) {
            return -1;
        }
        memset(&(bucket->entries[len]), 0, sizeof(Entry*) * (cap - len));
        bucket->cap = cap;
    }

    /* set the key */
    bucket->entries[len].key = calloc(key_len, sizeof(uint8_t*));
    if (bucket->entries[len].key == NULL) {
        return -1;
    }
    memcpy(bucket->entries[len].key, key, key_len);

    /* set the value */
    bucket->entries[len].value = malloc(val_size);
    if (bucket->entries[len].value == NULL) {
        free(bucket->entries[len].key);
        return -1;
    }
    memset(bucket->entries[len].value, 0, val_size);
    memcpy(bucket->entries[len].value, value, val_size);

    /* set the metadata and free callback*/
    bucket->entries[len].key_len = key_len;
    bucket->entries[len].val_size = val_size;
    bucket->entries[len].cb = cb;
    bucket->len++;
    return 0;
}

int ht_try_insert(Ht* ht, uint8_t* key, size_t key_len, void* value, size_t val_size, FreeCallBack* cb) {
    HtHasResult has = {0};
    int insert_result;

    if (cb == NULL) {
        errno = EINVAL;
        return -1;
    }

    ht_internal_has(ht, key, key_len, &has);


    if (has.has) {
        Bucket bucket = ht->buckets[has.hash];
        Entry e = bucket.entries[has.insert];

        /* free old value and assign new one */
        e.cb(e.value);
        e.value = malloc(val_size);
        if (e.value == NULL) {
            errno = ENOMEM;
            return -1;
        }
        memcpy(e.value, value, val_size);

        /* assign new metadata */
        e.val_size = val_size;
        e.cb = cb;

        return 0;
    } else {
        return -1;
    }
    return 0;
}

/* insert a key and a value */
int ht_insert(Ht* ht, uint8_t* key, size_t key_len, void* value,
              size_t val_size, FreeCallBack* cb) {
    HtHasResult has = {0};
    int insert_result;

    if (cb == NULL) {
        errno = EINVAL;
        return -1;
    }

    ht_internal_has(ht, key, key_len, &has);

    if (has.has) {
        Bucket bucket = ht->buckets[has.hash];
        Entry e = bucket.entries[has.insert];

        /* free old value and assign new one */
        e.cb(e.value);
        e.value = malloc(val_size);
        if (e.value == NULL) {
            errno = ENOMEM;
            return -1;
        }
        memcpy(e.value, value, val_size);

        /* assign new metadata */
        e.val_size = val_size;
        e.cb = cb;

        return 0;
    }

    /* insert new entry */
    insert_result = bucket_insert(&(ht->buckets[has.hash]), key, key_len, value,
                                  val_size, cb);
    if (insert_result == 0) {
        ht->len++;
        return 0;
    }
    return insert_result;
}

/* get a value */
void* ht_get(Ht* ht, uint8_t* key, size_t key_len) {
    HtHasResult has = {0};
    void* res;

    ht_internal_has(ht, key, key_len, &has);

    if (has.has == 0) {
        return NULL;
    }

    res = ht->buckets[has.hash].entries[has.insert].value;

    return res;
}

/* delete an entry from a bucket */
static void bucket_remove_entry(Bucket* bucket, size_t idx) {
    free_entry(&(bucket->entries[idx]));
    memmove(&(bucket->entries[idx]), &(bucket->entries[idx + 1]),
            sizeof(Entry) * (bucket->len - idx));
    bucket->len--;
}

/* delete an entry */
int ht_delete(Ht* ht, uint8_t* key, size_t key_len) {
    HtHasResult has = {0};

    ht_internal_has(ht, key, key_len, &has);

    if (has.has == 0) {
        return -1;
    }

    bucket_remove_entry(&(ht->buckets[has.hash]), has.insert);
    ht->len--;

    return 0;
}

static void entry_print(Entry* e) {
    size_t i;
    uint8_t* key = e->key;
    size_t key_len = e->key_len;
    void* value = e->value;
    size_t value_size = e->val_size;

    printf("(key: ");
    for (i = 0; i < key_len; ++i) {
        printf("%c", key[i]);
    }

    printf("): %p (size: %lu)\n", value, value_size);
}

static void bucket_print(Bucket* bucket) {
    size_t i, len;
    len = bucket->len;
    for (i = 0; i < len; ++i) {
        Entry e = bucket->entries[i];
        printf("\t");
        entry_print(&e);
    }
    printf("\n");
}

void ht_print(Ht* ht) {
    size_t i, len;
    len = ht->cap;
    for (i = 0; i < len; ++i) {
        Bucket bucket = ht->buckets[i];
        if (bucket.len > 0) {
            printf("[slot: %lu]\n", i);
            bucket_print(&bucket);
        }
    }
    printf("\n");
}

static void free_entry(Entry* e) {
    e->cb(e->value);
    free(e->key);
    e->value = NULL;
    e->key = NULL;
    e->key_len = 0;
    e->val_size = 0;
}

static void free_bucket(Bucket* bucket) {
    size_t i, len;
    len = bucket->len;
    for (i = 0; i < len; ++i) {
        Entry e = bucket->entries[i];
        free_entry(&e);
    }
    free(bucket->entries);
    bucket->entries = NULL;
    bucket->len = 0;
}

void ht_free(Ht* ht) {
    size_t i, len;
    len = ht->cap;
    for (i = 0; i < len; ++i) {
        Bucket b = ht->buckets[i];
        free_bucket(&b);
    }
    free(ht->buckets);
    ht->buckets = NULL;
    free(ht);
    ht = NULL;
}
