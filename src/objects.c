#include "objects.h"
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Object object_new(ObjectT type, void* v, size_t val_len) {
    Object obj;

    if (type == STRING) {
        obj.data.str = string_from(v, val_len);
    } else if (type == OINT) {
        obj.data.i64 = ((int64_t)v);
    } else {
        obj.data.null = NULL;
    }
    obj.type = type;
    return obj;
}

void object_free(Object* obj) {
    ObjectT objt;
    objt = obj->type;

    if (objt == STRING) {
        string_free(obj->data.str);
    }
    memset(obj, 0, sizeof *obj);
}

void string_print(String* str) {
    printf("%s\n", str->data);
}

String* string_new(size_t initial_cap) {
    String* str;
    size_t needed_size;

    if (initial_cap == 0) {
        errno = EINVAL;
        return NULL;
    }
    needed_size = sizeof(String) + initial_cap;
    str = malloc(needed_size);
    if (str == NULL) {
        return NULL;
    }
    memset(str, 0, needed_size);
    str->cap = initial_cap;
    return str;
}

String* string_from(char* value, size_t len) {
    String* str;
    size_t needed_len;
    needed_len = (sizeof *str) + len + 1;
    str = malloc(needed_len);
    if (str == NULL) {
        return NULL;
    }
    memset(str, 0, needed_len);

    memcpy(str->data, value, len);
    str->cap = len + 1;
    str->len = len;
    return str;
}

int string_push_char(String** str, char c) {
    size_t cap, len;
    cap = (*str)->cap;
    len = (*str)->len;
    if (len == (cap - 1)) {
        cap <<= 1; // multpily by two
        *str = realloc(*str, (sizeof(String)) + cap);
        if (*str == NULL) {
            return -1;
        }
        (*str)->cap = cap;
        memset((*str)->data + len, 0, cap - len);
    }
    (*str)->data[len] = c;
    (*str)->len++;
    return 0;
}

void string_free(String* str) {
    free(str);
    str = NULL;
}

Vec* vec_new(size_t initial_cap, size_t data_size) {
    Vec* vec;
    size_t needed_size;

    needed_size = sizeof(Vec) + (data_size * initial_cap);

    vec = malloc(needed_size);
    if (vec == NULL) {
        return NULL;
    }

    memset(vec, 0, needed_size);

    vec->data_size = data_size;
    vec->cap = initial_cap;

    return vec;
}

int vec_push(Vec** vec, void* data) {
    size_t len, cap, size, len_x_size;
    len = (*vec)->len;
    cap = (*vec)->cap;
    size = (*vec)->data_size;
    len_x_size = len * size;

    if (len == cap) {
        size_t cap_x_size, needed_size;
        cap <<= 1;
        cap_x_size = cap * size;
        needed_size = sizeof(Vec) + cap_x_size;
        *vec = realloc(*vec, needed_size);
        if (*vec == NULL) {
            return -1;
        }

        memset((*vec)->data + len_x_size, 0, cap_x_size - len_x_size);

        (*vec)->cap = cap;
    }

    memcpy((*vec)->data + len_x_size, data, size);

    (*vec)->len++;

    return 0;
}

void vec_for_each(Vec* vec, VecForEach* fn) {
    size_t i, len, size;
    len = vec->len;
    size = vec->data_size;

    for (i = 0; i < len; ++i) {
        void* item = vec->data + (i * size);
        fn(item);
    }

}

void vec_free(Vec* vec, VecFreeCallBack* cb) {
    size_t i, len, size;
    len = vec->len;
    size = vec->data_size;

    if (cb) {
        for (i = 0; i < len; ++i) {
            void* item = vec->data + (i * size);
            cb(item);
        }
    }

    free(vec);
}
