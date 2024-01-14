#include "object.h"
#include <stdio.h>

static void free_object_in_vec(void* ptr);

object object_new(objectt type, void* data) {
    object obj = {0};
    switch (type) {
    case Null:
        obj.type = Null;
        break;
    case Int:
        obj.type = Int;
        obj.data.num = *(int64_t*)data;
        break;
    case Double:
        obj.type = Double;
        obj.data.dbl = *(double*)data;
        break;
    case String:
        obj.type = String;
        obj.data.string = *(vstr*)data;
        break;
    case Array:
        obj.type = Array;
        obj.data.vec = *(vec**)data;
        break;
    default:
        obj.type = Null;
        break;
    }
    return obj;
}

int object_cmp(object* a, object* b) {
    if (a->type != b->type) {
        return a->type - b->type;
    }
    switch (a->type) {
    case Null:
        return 0;
    case Int: {
        int64_t ai = a->data.num;
        int64_t bi = b->data.num;
        return ai - bi;
    }
    case String: {
        vstr as = a->data.string;
        vstr bs = b->data.string;
        return vstr_cmp(&as, &bs);
    }
    default:
        return 1;
    }
    return 0;
}

void object_show(object* obj) {
    switch (obj->type) {
    case Null:
        printf("null\n");
        break;
    case Int:
        printf("%ld\n", obj->data.num);
        break;
    case Double:
        printf("%f\n", obj->data.dbl);
        break;
    case String:
        printf("%s\n", vstr_data(&(obj->data.string)));
        break;
    case Array:{
        vec_iter iter = vec_iter_new(obj->data.vec);
        while (iter.cur) {
            object* obj = iter.cur;
            object_show(obj);
            vec_iter_next(&iter);
        }
    } break;
    }
}

void object_free(object* obj) {
    switch (obj->type) {
    case Null:
        break;
    case Int:
        break;
    case String:
        vstr_free(&(obj->data.string));
        break;
    case Array:
        vec_free(obj->data.vec, free_object_in_vec);
        break;
    default:
        break;
    }
}

static void free_object_in_vec(void* ptr) {
    object* obj = ptr;
    object_free(obj);
}
