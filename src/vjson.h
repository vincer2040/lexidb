#ifndef __VJSON_H__

#define __VJSON_H__

#include "ht.h"
#include "vec.h"
#include "vstr.h"

typedef enum {
    JOT_Null,
    JOT_Number,
    JOT_Bool,
    JOT_String,
    JOT_Array,
    JOT_Object,
} vjson_object_t;

typedef struct {
    vjson_object_t type;
    union {
        double number;
        int boolean;
        vstr string;
        vec* array;
        ht object;
    } data;
} vjson_object;

vjson_object* vjson_parse(const unsigned char* input, size_t input_len);

void vjson_object_free(vjson_object* obj);

vstr vjson_object_to_string(vjson_object* obj);

#endif
