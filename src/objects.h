#ifndef __OBJECTS_H__

#define __OBJECTS_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t len;
    size_t cap;
    char data[];
} String;

typedef enum {
    SNULL,
    STRING
} ObjectT;

typedef union {
    String* str;
    void* null;
} ObjectD;

typedef struct {
    ObjectT type;
    ObjectD data;
} Object;

Object object_new(ObjectT type, char* v, size_t val_len);
void object_free(Object* obj);
String* string_new(size_t initial_cap);
String* string_from(char* value, size_t len);
void string_print(String* str);
int string_push_char(String** str, char c);
void string_free(String* str);

#endif
