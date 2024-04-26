#ifndef __VEC_H__

#define __VEC_H__

#include <stddef.h>

typedef struct vec vec;

vec* vec_new(size_t data_size);
size_t vec_len(const vec* v);
int vec_push(vec** v, const void* data);
int vec_pop(vec* v, void* out);
const void* vec_get_at(const vec* v, size_t idx);
const void* vec_find(const vec* v, const void* el,
                     int (*cmp_fn)(const void*, const void*));
int vec_remove(vec* v, const void* el, int (*cmp_fn)(const void*, const void*),
               void (*free_fn)(void*));
void vec_free(vec* v, void (*free_fn)(void*));

typedef struct {
    const vec* vec;
    const void* cur;
    const void* next;
    size_t pos;
    size_t len;
    size_t data_size;
} vec_iter;

vec_iter vec_iter_new(const vec* vec);
void vec_iter_next(vec_iter* iter);

#endif /* __VEC_H__ */
