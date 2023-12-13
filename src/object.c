#include "object.h"

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
    case String:
        obj.type = String;
        obj.data.string = *(vstr*)data;
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
    }
    return 0;
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
    default:
        break;
    }
}
