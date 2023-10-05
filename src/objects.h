#ifndef __OBJECTS_H__

#define __OBJECTS_H__

#include "vstring.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ONULL,
    OINT64,
    STRING
} ObjectT;

typedef union {
    vstr str;
    int64_t i64;
    void* null;
} ObjectD;

typedef struct {
    ObjectT type;
    ObjectD data;
} Object;

/* generic object */
Object object_new(ObjectT type, void* v, size_t val_len);
void object_free(Object* obj);

#endif
