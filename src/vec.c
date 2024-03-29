#include "vec.h"
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>

#define VEC_INITIAL_CAP 2

vec* vec_new(size_t data_size) {
    vec* vec;
    size_t needed = (sizeof *vec) + (data_size * VEC_INITIAL_CAP);
    vec = malloc(needed);
    if (vec == NULL) {
        return NULL;
    }
    memset(vec, 0, needed);
    vec->cap = VEC_INITIAL_CAP;
    vec->data_size = data_size;
    return vec;
}

vec* vec_new_with_capacity(size_t data_size, size_t capacity) {
    vec* vec;
    size_t needed;
    if (capacity < 2) {
        return NULL;
    }
    needed = (sizeof *vec) + (data_size * capacity);
    vec = malloc(needed);
    if (vec == NULL) {
        return NULL;
    }
    memset(vec, 0, needed);
    vec->cap = capacity;
    vec->data_size = data_size;
    return vec;
}

static int vec_realloc(vec** vec, size_t cap, size_t len, size_t len_x_size,
                       size_t data_size) {
    void* tmp;
    cap <<= 1;
    tmp = realloc(*vec, sizeof(**vec) + (cap * data_size));
    if (tmp == NULL) {
        return -1;
    }
    *vec = tmp;
    memset((*vec)->data + len_x_size, 0, (cap - len) * data_size);
    (*vec)->cap = cap;
    return 0;
}

int vec_push(vec** vec, void* data) {
    size_t len = (*vec)->len, cap = (*vec)->cap, data_size = (*vec)->data_size;
    size_t len_x_size = len * data_size;
    if (len == cap) {
        int vec_realloc_res = vec_realloc(vec, cap, len, len_x_size, data_size);
        if (vec_realloc_res == -1) {
            return -1;
        }
    }
    memcpy((*vec)->data + len_x_size, data, data_size);
    (*vec)->len += 1;
    return 0;
}

int vec_pop(vec* vec, void* out) {
    size_t pop_idx, pop_idx_x_size, data_size = vec->data_size;
    if (vec->len == 0) {
        return -1;
    }
    pop_idx = vec->len - 1;
    pop_idx_x_size = pop_idx * data_size;
    memcpy(out, vec->data + pop_idx_x_size, data_size);
    vec->len--;
    return 0;
}

void* vec_get_at(vec* vec, size_t idx) {
    size_t len = vec->len, data_size = vec->data_size;
    if (idx >= len) {
        return NULL;
    }
    return vec->data + (idx * data_size);
}

int vec_remove_at(vec* vec, size_t idx, void* out) {
    size_t idx_x_size, new_len, new_len_x_size, len = vec->len,
                                                data_size = vec->data_size;
    if (idx >= len) {
        return -1;
    }
    idx_x_size = idx * data_size;
    new_len = len - 1;
    vec->len--;
    if (out) {
        memcpy(out, vec->data + idx_x_size, data_size);
    }
    if (new_len == idx) {
        memset(vec->data + idx_x_size, 0, data_size);
        return 0;
    }
    new_len_x_size = new_len * data_size;
    memmove(vec->data + idx_x_size, vec->data + ((idx + 1) * data_size),
            new_len_x_size - idx_x_size);
    memset(vec->data + new_len_x_size, 0, data_size);
    return 0;
}

ssize_t vec_find(vec* vec, void* cmp_data, void* out, cmp_fn* fn) {
    size_t i, len = vec->len, data_size = vec->data_size;
    for (i = 0; i < len; ++i) {
        void* at = vec->data + (i * data_size);
        int cmp_res = fn(cmp_data, at);
        if (cmp_res == 0) {
            if (out) {
                memcpy(out, at, data_size);
            }
            return i;
        }
    }
    return -1;
}

void vec_free(vec* vec, free_fn* fn) {
    if (fn) {
        size_t i, len = vec->len, data_size = vec->data_size;
        for (i = 0; i < len; ++i) {
            void* at = vec->data + (i * data_size);
            fn(at);
        }
    }
    free(vec);
}

vec_iter vec_iter_new(vec* vec) {
    vec_iter iter = {0};
    iter.next_idx = 0;
    iter.end_idx = vec->len - 1;
    iter.vec = vec;
    vec_iter_next(&iter);
    vec_iter_next(&iter);
    return iter;
}

void vec_iter_next(vec_iter* iter) {
    iter->cur = iter->next;
    if (iter->next_idx > iter->end_idx) {
        iter->next = NULL;
    } else {
        iter->next = iter->vec->data + (iter->vec->data_size * iter->next_idx);
    }
    iter->next_idx++;
}
