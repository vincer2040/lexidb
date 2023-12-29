#ifndef __OBJECT_H__

#define __OBJECT_H__

#include "vstr.h"
#include "vec.h"
#include <stdint.h>

typedef enum {
    Null,
    Int,
    String,
    Array,
} objectt;

typedef struct {
    objectt type;
    union {
        int64_t num;
        vstr string;
        vec* vec;
    } data;
} object;

object object_new(objectt type, void* data);
int object_cmp(object* a, object* b);
void object_show(object* obj);
void object_free(object* obj);

#endif /* __OBJECT_H__ */
