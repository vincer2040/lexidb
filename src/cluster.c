#include "cluster.h"
#include "ht.h"
#include "objects.h"
#include <stdint.h>
#include <stdlib.h>

Cluster* cluster_new(size_t initial_cap) { return ht_new(initial_cap, sizeof(ClusterDB)); }

void cfree_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
    free(obj);
}

static ClusterDB* clusterdb_new(size_t ht_initial_cap, size_t vec_initial_cap) {
    ClusterDB* cdb;

    cdb = calloc(1, sizeof *cdb);
    if (cdb == NULL) {
        return NULL;
    }

    cdb->ht = ht_new(ht_initial_cap, sizeof(Object));
    if (cdb->ht == NULL) {
        free(cdb);
        return NULL;
    }

    cdb->vec = vec_new(vec_initial_cap, sizeof(Object));
    if (cdb->vec == NULL) {
        ht_free(cdb->ht, cfree_cb);
        free(cdb);
        return NULL;
    }

    return cdb;
}

void cluster_vec_free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
}

void clusterdb_free(void* ptr) {
    ClusterDB* db = *((ClusterDB**)(ptr));

    ht_free(db->ht, cfree_cb);
    vec_free(db->vec, cluster_vec_free_cb);
    free(db);
    free(ptr);
}

int cluster_namespace_new(Cluster* cluster, uint8_t* key, size_t key_len,
                          size_t initial_cap) {
    uint8_t has;
    ClusterDB* namespace;

    has = ht_has(cluster, key, key_len);

    if (has) {
        return 1;
    }

    namespace = clusterdb_new(initial_cap, initial_cap);

    if (namespace == NULL) {
        return -1;
    }

    return ht_insert(cluster, key, key_len, &namespace,
                     clusterdb_free);
}

int cluster_namespace_insert(Cluster* cluster, uint8_t* cluster_key,
                             size_t cluster_key_len, uint8_t* key,
                             size_t key_len, void* value,
                             FreeCallBack* cb) {
    ClusterDB* cdb;
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    cdb = *((ClusterDB**)ptr);
    ht = cdb->ht;

    return ht_insert(ht, key, key_len, value, cb);
}

void* cluster_namespace_get(Cluster* cluster, uint8_t* cluster_key,
                            size_t cluster_key_len, uint8_t* key,
                            size_t key_len) {
    ClusterDB* cdb;
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return NULL;
    }

    cdb = *((ClusterDB**)ptr);
    ht = cdb->ht;

    return ht_get(ht, key, key_len);
}

int cluster_namespace_del(Cluster* cluster, uint8_t* cluster_key,
                          size_t cluster_key_len, uint8_t* key,
                          size_t key_len, FreeCallBack* fcb) {
    ClusterDB* cdb;
    Ht* ht;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    cdb = *((ClusterDB**)ptr);
    ht = cdb->ht;

    return ht_delete(ht, key, key_len, fcb);
}

size_t cluster_namespace_len(Cluster* cluster, uint8_t* cluster_key,
                             size_t cluster_key_len) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return 0;
    }

    cdb = *((ClusterDB**)ptr);

    return ht_len(cdb->ht);
}

int cluster_namespace_push(Cluster* cluster, uint8_t* cluster_key,
                           size_t cluster_key_len, void* value) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    cdb = *((ClusterDB**)ptr);

    return vec_push(&(cdb->vec), value);
}

int cluster_namespace_pop(Cluster* cluster, uint8_t* cluster_key,
                          size_t cluster_key_len, void* out) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return -1;
    }

    cdb = *((ClusterDB**)ptr);

    return vec_pop(cdb->vec, out);
}

int cluster_namespace_drop(Cluster* cluster, uint8_t* key, size_t key_len) {
    return ht_delete(cluster, key, key_len, clusterdb_free);
}

HtKeysIter* cluster_namespace_keys_iter(Cluster* cluster, uint8_t* cluster_key,
                                        size_t cluster_key_len) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);

    if (ptr == NULL) {
        return NULL;
    }

    cdb = *((ClusterDB**)ptr);

    return ht_keys_iter(cdb->ht);
}

HtValuesIter* cluster_namespace_values_iter(Cluster* cluster,
                                            uint8_t* cluster_key,
                                            size_t cluster_key_len) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);
    if (ptr == NULL) {
        return NULL;
    }

    cdb = *((ClusterDB**)ptr);

    return ht_values_iter(cdb->ht);
}

HtEntriesIter* cluster_namespace_entries_iter(Cluster* cluster,
                                              uint8_t* cluster_key,
                                              size_t cluster_key_len) {
    ClusterDB* cdb;
    void* ptr = ht_get(cluster, cluster_key, cluster_key_len);
    if (ptr == NULL) {
        return NULL;
    }

    cdb = *((ClusterDB**)ptr);

    return ht_entries_iter(cdb->ht);
}
