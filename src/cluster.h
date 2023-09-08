#ifndef __CLUSTER_H__

#define __CLUSTER_H__

#include "ht.h"
#include "objects.h"
#include "vec.h"

typedef Ht Cluster;

typedef struct {
    Ht* ht;
    Vec* vec;
} ClusterDB;

Cluster* cluster_new(size_t initial_cap);

int cluster_namespace_new(Cluster* cluster, uint8_t* key, size_t key_len,
                          size_t initial_cap);
void* cluster_namespace_get(Cluster* cluster, uint8_t* cluster_key,
                            size_t cluster_key_len, uint8_t* key,
                            size_t key_len);
int cluster_namespace_insert(Cluster* cluster, uint8_t* cluster_key,
                             size_t cluster_key_len, uint8_t* key,
                             size_t key_len, void* value, size_t value_size,
                             FreeCallBack* cb);
int cluster_namespace_del(Cluster* cluster, uint8_t* cluster_key,
                          size_t cluster_key_len, uint8_t* key, size_t key_len);
int cluster_namespace_push(Cluster* cluster, uint8_t* cluster_key,
                           size_t cluster_key_len, void* value);
int cluster_namespace_pop(Cluster* cluster, uint8_t* cluster_key,
                          size_t cluster_key_len, void* out);
int cluster_namespace_drop(Cluster* cluster, uint8_t* key, size_t key_len);
HtKeysIter* cluster_namespace_keys_iter(Cluster* cluster, uint8_t* cluster_key,
                                   size_t cluster_key_len);
HtValuesIter* cluster_namespace_values_iter(Cluster* cluster,
                                            uint8_t* cluster_key,
                                            size_t cluster_key_len);
HtEntriesIter* cluster_namespace_entries_iter(Cluster* cluster,
                                              uint8_t* cluster_key,
                                              size_t cluster_key_len);

#define cluster_free(cluster) ht_free((cluster));

#endif
