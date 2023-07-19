#ifndef __CLUSTER_H__

#define __CLUSTER_H__

#include "ht.h"

typedef Ht Cluster;

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
                            size_t cluster_key_len, uint8_t* key,
                            size_t key_len);

int cluster_namespace_drop(Cluster* cluster, uint8_t* key, size_t key_len);

#define cluster_free(cluster) ht_free(cluster);

#endif
