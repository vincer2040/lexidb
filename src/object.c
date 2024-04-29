#include "object.h"

object null_obj = {0};
object true_obj = {.type = OT_Boolean, .data = {.boolean = 1}};
object false_obj = {.type = OT_Boolean, .data = {.boolean = 0}};

const char* object_type_strings[] = {
    "Null",
    "Int",
    "Double",
    "String",
    "Error",
};

const char* object_type_to_string(const object* obj) {
    return object_type_strings[obj->type];
}

object object_to_string(const object* obj) {
    object res = {0};
    switch (obj->type) {
    case OT_Null:
        res.data.string = vstr_from("null");
        break;
    case OT_Int:
        res.data.string = vstr_format("%ld", obj->data.integer);
        break;
    case OT_Double:
        res.data.string = vstr_format("%g", obj->data.dbl);
        break;
    case OT_Boolean:
        res.data.string = vstr_from(obj->data.boolean ? "true" : "false");
        break;
    case OT_String:
        res.data.string = vstr_from_len(vstr_data(&obj->data.string), vstr_len(&obj->data.string));
        break;
    case OT_Error:
        res.data.string = vstr_from_len(vstr_data(&obj->data.string), vstr_len(&obj->data.string));
        break;
    }
    res.type = OT_String;
    return res;
}

int object_cmp(const object* a, const object* b) {
    if (a->type != b->type) {
        return a->type - b->type;
    }
    switch (a->type) {
    case OT_Null:
        return 0;
    case OT_Int:
        return a->data.integer - b->data.integer;
    case OT_Double:
        return a->data.dbl - b->data.dbl;
    case OT_Boolean:
        return a->data.boolean - b->data.boolean;
    case OT_String:
        return vstr_cmp(&a->data.string, &b->data.string);
    case OT_Error:
        return vstr_cmp(&a->data.string, &b->data.string);
    }
    return -1;
}

void object_free(object* obj) {
    switch (obj->type) {
    case OT_Error:
        vstr_free(&obj->data.string);
        break;
    case OT_String:
        vstr_free(&obj->data.string);
        break;
    default:
        break;
    }
}
