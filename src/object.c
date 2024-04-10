#include "object.h"

object null_obj = {0};
object true_obj = {.type = Boolean, .data = {.boolean = 1}};
object false_obj = {.type = Boolean, .data = {.boolean = 0}};

void object_free(object* obj) {
    switch (obj->type) {
    case Error:
        vstr_free(&obj->data.string);
        break;
    case String:
        vstr_free(&obj->data.string);
        break;
    default:
        break;
    }
}
