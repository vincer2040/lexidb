#ifndef __OBJECT_H__

#define __OBJECT_H__

#include "ht.h"
#include "vstr.h"
#include "vec.h"
#include <stdint.h>

typedef enum {
    Null,
    Int,
    Double,
    String,
    Array,
    Ht,
} objectt;

typedef struct {
    objectt type;
    union {
        int64_t num;
        double dbl;
        vstr string;
        vec* vec;
        ht ht;
    } data;
} object;

object object_new(objectt type, void* data);
int object_cmp(object* a, object* b);
void object_show(object* obj);
void object_free(object* obj);

#endif /* __OBJECT_H__ */
