#ifndef __VJSON_H__

#define __VJSON_H__

#include "vstr.h"

typedef enum {
    JOT_String,
    JOT_Number,
    JOT_Object,
} json_object_t;

typedef struct json_object {
    json_object_t type;
    union {
        vstr string;
        double number;
        struct json_object* object;
    } data;
} json_object;

json_object* vjson_parse(const unsigned char* input, size_t input_len);

void vjson_object_free(json_object* obj);

#endif
