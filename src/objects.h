#ifndef __OBJECTS_H__

#define __OBJECTS_H__

#include "vstring.h"
#include <stddef.h>
#include <stdint.h>

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

typedef enum {
    ONULL,
    OINT,
    STRING
} ObjectT;

typedef union {
    vstr str;
    int64_t i64;
    void* null;
} ObjectD;

typedef struct {
    ObjectT type;
    ObjectD data;
} Object;

/* generic object */
Object object_new(ObjectT type, void* v, size_t val_len);
void object_free(Object* obj);

/* vector */
Vec* vec_new(size_t initial_cap, size_t data_size);
int vec_push(Vec** vec, void* data);
int vec_pop(Vec* vec, void* data);
void vec_for_each(Vec* vec, VecForEach* fn);
int vec_remove(Vec* vec, void* cmp_against, VecCmpFn* fn);
void vec_free(Vec* vec, VecFreeCallBack* cb);

/* vec iterator */
VecIter vec_iter_new(Vec* vec);
void vec_iter_next(VecIter* iter);
void vec_iter_free(VecIter* iter);

#endif
