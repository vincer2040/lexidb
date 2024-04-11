#include "vec.h"
#include <memory.h>
#include <stddef.h>
#include <stdlib.h>

#define VEC_INITIAL_CAP 2

struct vec {
    size_t len;
    size_t cap;
    size_t data_size;
    unsigned char data[];
};

static int vec_resize(vec** v, size_t new_cap);

vec* vec_new(size_t data_size) {
    vec* v;
    size_t needed = (sizeof *v) + (data_size * VEC_INITIAL_CAP);
    v = malloc(needed);
    memset(v, 0, needed);
    v->cap = VEC_INITIAL_CAP;
    v->data_size = data_size;
    return v;
}

size_t vec_len(const vec* v) { return v->len; }

int vec_push(vec** v, const void* data) {
    size_t len = (*v)->len, cap = (*v)->cap, data_size = (*v)->data_size;
    if (len >= cap) {
        int resize = vec_resize(v, (cap << 1));
        if (resize == -1) {
            return -1;
        }
    }
    memcpy((*v)->data + (len * data_size), data, data_size);
    (*v)->len++;
    return 0;
}

int vec_pop(vec* v, void* out) {
    size_t pos, data_size;
    if (v->len == 0) {
        return -1;
    }
    data_size = v->data_size;
    pos = (v->len - 1) * data_size;
    if (out) {
        memcpy(out, v->data + pos, data_size);
    }
    memset(v->data + pos, 0, data_size);
    v->len--;
    return 0;
}

const void* vec_find(const vec* v, const void* el, int (*cmp_fn)(const void*, const void*)) {
    size_t i, len = v->len, data_size = v->data_size;
    for (i = 0; i < len; ++i) {
        const void* cur = v->data + (i * data_size);
        int cmp = cmp_fn(el, cur);
        if (cmp == 0) {
            return cur;
        }
    }
    return NULL;
}

void vec_free(vec* v, void (*free_fn)(void*)) {
    if (free_fn) {
        size_t i, len = v->len, data_size = v->data_size;
        for (i = 0; i < len; ++i) {
            void* cur = v->data + (i * data_size);
            free_fn(cur);
        }
    }
    free(v);
}

static int vec_resize(vec** v, size_t new_cap) {
    void* tmp;
    size_t needed, cur_len = (*v)->len, data_size = (*v)->data_size;
    needed = (sizeof **v) + (data_size * new_cap);
    tmp = realloc(*v, needed);
    if (tmp == NULL) {
        return -1;
    }
    *v = tmp;
    memset((*v)->data + (data_size * cur_len), 0,
           ((new_cap - cur_len) * data_size));
    return 0;
}
