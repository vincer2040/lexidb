#ifndef __MAP_H__

#define __MAP_H__

#include <stdint.h>
#include <stddef.h>

typedef struct vmap vmap;

#define VMAP_OK 0
#define VMAP_NO_KEY 1
#define VMAP_OOM 2

typedef struct {
    uint64_t (*hash)(const void* key);
    int (*key_cmp)(const void* a, const void* b);
    void (*key_free)(void* key);
    void (*value_free)(void* value);
    size_t key_size;
    size_t value_size;
} vmap_type;

vmap* vmap_new(vmap_type* type);
int vmap_insert(vmap** map, void* key, void* value);
const void* vmap_find(const vmap* map, const void* key);
int vmap_delete(vmap** map, const void* key);
void vmap_free(vmap* map);

#endif /* __MAP_H__ */
