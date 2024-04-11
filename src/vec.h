#ifndef __VEC_H__

#define __VEC_H__

#include <stddef.h>

typedef struct vec vec;

vec* vec_new(size_t data_size);
size_t vec_len(const vec* v);
int vec_push(vec** v, const void* data);
int vec_pop(vec* v, void* out);
void vec_free(vec* v, void (*free_fn)(void*));

#endif /* __VEC_H__ */
