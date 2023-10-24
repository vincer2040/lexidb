#include "objects.h"
#include "vstring.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Object object_new(ObjectT type, void* v, size_t val_len) {
    Object obj;

    if (type == STRING) {
        obj.data.str = vstr_new_len(val_len);
        obj.data.str = vstr_push_string_len(obj.data.str, v, val_len);
    } else if (type == OINT64) {
        obj.data.i64 = ((int64_t)v);
    } else {
        obj.data.null = NULL;
    }
    obj.type = type;
    return obj;
}

void object_free(Object* obj) {
    ObjectT objt;
    objt = obj->type;

    if (objt == STRING) {
        vstr_delete(obj->data.str);
    }
    memset(obj, 0, sizeof *obj);
}

void object_print(Object* obj) {
    switch (obj->type) {
    case STRING:
        printf("%s\n", obj->data.str);
        break;
    case OINT64:
        printf("%ld\n", obj->data.i64);
        break;
    case ONULL:
        printf("null\n");
        break;
    }
}
