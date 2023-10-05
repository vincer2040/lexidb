#ifndef __VEC_H__

#define __VEC_H__

#include <stddef.h>

typedef void VecFreeCallBack(void* ptr);
typedef void VecForEach(void* ptr);
typedef int VecCmpFn(void* cmp_against, void* cur);

typedef struct {
    size_t len;
    size_t cap;
    size_t data_size;
    unsigned char data[];
} Vec;

typedef struct {
    void* start;
    void* end;
    void* cur;
    void* next;
    size_t at_idx;
    Vec* vec;
} VecIter;

/* vector */
Vec* vec_new(size_t initial_cap, size_t data_size);
int vec_push(Vec** vec, void* data);
int vec_pop(Vec* vec, void* data);
void vec_for_each(Vec* vec, VecForEach* fn);
int vec_remove(Vec* vec, void* cmp_against, VecCmpFn* fn);
void vec_free(Vec* vec, VecFreeCallBack* cb);
int vec_find(Vec* vec, void* cmp_against, VecCmpFn* fn, void* out);

/* vec iterator */
VecIter vec_iter_new(Vec* vec);
void vec_iter_next(VecIter* iter);

#endif /* __VEC_H__ */
