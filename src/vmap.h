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
int vmap_has(const vmap* map, const void* key);
int vmap_delete(vmap** map, const void* key);
void vmap_free(vmap* map);

typedef unsigned char vmap_entry;

typedef struct {
    const vmap* map;
    const vmap_entry* cur;
    const vmap_entry* next;
    uint64_t pos;
    uint64_t len;
} vmap_iter;

vmap_iter vmap_iter_new(const vmap* map);
void vmap_iter_next(vmap_iter* iter);

const void* vmap_entry_get_key(const vmap_entry* e);
const void* vmap_entry_get_value(const vmap* map, const vmap_entry* e);

#endif /* __MAP_H__ */
