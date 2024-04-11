#ifndef __MAP_H__

#define __MAP_H__

#include <stdint.h>
#include <stddef.h>

typedef struct vmap vmap;

typedef struct {
    uint64_t (*hash)(void* key);
    int (*key_cmp)(void* a, void* b);
    void (*key_free)(void* key);
    void (*value_free)(void* value);
    size_t key_size;
    size_t value_size;
} vmap_type;

#endif /* __MAP_H__ */
