#include "object.h"
#include "ht.h"
#include <assert.h>
#include <stdio.h>

static void free_object_in_structure(void* ptr);
static void object_show_no_newline(object* obj);

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
    case Bool:
        obj.type = Bool;
        obj.data.boolean = *(int*)data;
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
    case Double: {
        double ad = a->data.dbl;
        double bd = a->data.dbl;
        return ad - bd;
    }
    case Bool: {
        int ab = a->data.boolean;
        int bb = a->data.boolean;
        return ab - bb;
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
    case Bool:
        if (obj->data.boolean) {
            printf("t\n");
        } else {
            printf("f\n");
        }
    case String:
        printf("%s\n", vstr_data(&(obj->data.string)));
        break;
    case Array: {
        vec_iter iter = vec_iter_new(obj->data.vec);
        while (iter.cur) {
            object* obj = iter.cur;
            object_show(obj);
            vec_iter_next(&iter);
        }
    } break;
    case Ht: {
        ht_iter iter = ht_iter_new(&obj->data.ht);
        while (iter.cur) {
            ht_entry* cur = iter.cur;
            object* key = (object*)ht_entry_get_key(cur);
            object* value = (object*)ht_entry_get_value(cur);
            object_show_no_newline(key);
            printf(": ");
            object_show(value);
            ht_iter_next(&iter);
        }
    } break;
    }
}

void object_free(object* obj) {
    switch (obj->type) {
    case String:
        vstr_free(&(obj->data.string));
        break;
    case Array:
        vec_free(obj->data.vec, free_object_in_structure);
        break;
    case Ht:
        ht_free(&obj->data.ht, free_object_in_structure,
                free_object_in_structure);
        break;
    default:
        break;
    }
}

static void object_show_no_newline(object* obj) {
    switch (obj->type) {
    case Null:
        printf("null");
        break;
    case Int:
        printf("%ld", obj->data.num);
        break;
    case Double:
        printf("%f", obj->data.dbl);
        break;
    case Bool:
        if (obj->data.boolean) {
            printf("t");
        } else {
            printf("f");
        }
    case String:
        printf("%s", vstr_data(&(obj->data.string)));
        break;
    case Array: {
        vec_iter iter = vec_iter_new(obj->data.vec);
        size_t i = 0;
        size_t len = obj->data.vec->len;
        while (iter.cur) {
            object* obj = iter.cur;
            object_show_no_newline(obj);
            if (i != len - 1) {
                printf(", ");
            }
            vec_iter_next(&iter);
            i++;
        }
    } break;
    case Ht: {
        ht_iter iter = ht_iter_new(&obj->data.ht);
        size_t i = 0;
        size_t len = obj->data.ht.num_entries;
        printf("{");
        while (iter.cur) {
            ht_entry* cur = iter.cur;
            object* key = (object*)ht_entry_get_key(cur);
            object* value = (object*)ht_entry_get_value(cur);
            object_show_no_newline(key);
            printf(": ");
            object_show_no_newline(value);
            if (i != len - 1) {
                printf(", ");
            }
            ht_iter_next(&iter);
            i++;
        }
        printf("}");
    } break;
    }
}

static void free_object_in_structure(void* ptr) {
    object* obj = ptr;
    object_free(obj);
}
