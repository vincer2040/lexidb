#ifndef __OBJECT_H__

#define __OBJECT_H__

#include "vstr.h"

typedef enum {
    OT_Null,
    OT_Int,
    OT_Double,
    OT_Boolean,
    OT_String,
    OT_Error,
} object_type;

typedef struct {
    object_type type;
    union {
        int64_t integer;
        double dbl;
        int boolean;
        vstr string;
    } data;
} object;

extern object null_obj;
extern object true_obj;
extern object false_obj;

void object_free(object* obj);

#endif /* __OBJECT_H__ */
