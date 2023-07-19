#include "cluster.h"
#include "ht.h"
#include <stdint.h>
#include <stdlib.h>

Cluster* cluster_new(size_t initial_cap) { return ht_new(initial_cap); }

void cluster_free_fn(void* ptr) {
    Ht* ht = *((Ht**)ptr);

    ht_free(ht);
    free(ptr);
}

int cluster_namespace_new(Cluster* cluster, uint8_t* key, size_t key_len,
                          size_t initial_cap) {
    uint8_t has;
    Ht* namespace;

    has = ht_has(cluster, key, key_len);

    if (has) {
        return 1;
    }

    namespace = ht_new(initial_cap);

    if (namespace == NULL) {
        return -1;
    }

    return ht_insert(cluster, key, key_len, &namespace, sizeof(Ht*),
                     cluster_free_fn);
}

int cluster_namespace_insert(Cluster* cluster, uint8_t* cluster_key,
                             size_t cluster_key_len, uint8_t* key,
                             size_t key_len, void* value, size_t val_size,
                             FreeCallBack* cb) {
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    ht = *((Ht**)ptr);

    return ht_insert(ht, key, key_len, value, val_size, cb);
}

void* cluster_namespace_get(Cluster* cluster, uint8_t* cluster_key,
                            size_t cluster_key_len, uint8_t* key,
                            size_t key_len) {
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return NULL;
    }

    ht = *((Ht**)ptr);

    return ht_get(ht, key, key_len);
}

int cluster_namespace_del(Cluster* cluster, uint8_t* cluster_key,
                          size_t cluster_key_len, uint8_t* key,
                          size_t key_len) {
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    ht = *((Ht**)ptr);

    return ht_delete(ht, key, key_len);
}

int cluster_namespace_drop(Cluster* cluster, uint8_t* key, size_t key_len) {
    return ht_delete(cluster, key, key_len);
}
