#include "object.h"

object null_obj = {0};
object true_obj = {.type = OT_Boolean, .data = {.boolean = 1}};
object false_obj = {.type = OT_Boolean, .data = {.boolean = 0}};

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
