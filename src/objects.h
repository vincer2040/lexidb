#ifndef __OBJECTS_H__

#define __OBJECTS_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t len;
    size_t cap;
    char data[];
} String;

typedef void VecFreeCallBack(void* ptr);
typedef void VecForEach(void* ptr);

typedef struct {
    size_t len;
    size_t cap;
    size_t data_size;
    unsigned char data[];
} Vec;

typedef enum {
    ONULL,
    OINT,
    STRING
} ObjectT;

typedef union {
    String* str;
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

/* string */
String* string_new(size_t initial_cap);
String* string_from(char* value, size_t len);
void string_print(String* str);
int string_push_char(String** str, char c);
void string_free(String* str);

/* vector */
Vec* vec_new(size_t initial_cap, size_t data_size);
int vec_push(Vec** vec, void* data);
int vec_pop(Vec* vec, void* data);
void vec_for_each(Vec* vec, VecForEach* fn);
void vec_free(Vec* vec, VecFreeCallBack* cb);

#endif
