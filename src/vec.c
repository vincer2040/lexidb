#include "vec.h"
#include <stdlib.h>
#include <memory.h>

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
        void* tmp;
        cap <<= 1;
        cap_x_size = cap * size;
        needed_size = sizeof(Vec) + cap_x_size;
        tmp = realloc(*vec, needed_size);
        if (tmp == NULL) {
            return -1;
        }
        *vec = tmp;

        memset((*vec)->data + len_x_size, 0, cap_x_size - len_x_size);

        (*vec)->cap = cap;
    }

    memcpy((*vec)->data + len_x_size, data, size);

    (*vec)->len++;

    return 0;
}

int vec_pop(Vec* vec, void* out) {
    size_t len, data_size, len_x_size;
    void* o;

    if (vec->len == 0) {
        return -1;
    }

    len = vec->len - 1;
    data_size = vec->data_size;
    len_x_size = len * data_size;

    o = vec->data + len_x_size;
    memcpy(out, o, data_size);

    memset(vec->data + len_x_size, 0, data_size);
    vec->len--;

    return 0;
}

int vec_find(Vec* vec, void* cmp_against, VecCmpFn* fn, void* out) {
    size_t i, len, data_size;

    if (vec->len == 0) {
        return -1;
    }

    len = vec->len;
    data_size = vec->data_size;

    for (i = 0; i < len; ++i) {
        void* c = vec->data + (i * data_size);
        if (fn(cmp_against, c) == 0) {
            goto found;
        }
    }
    return -1;

found:
    memcpy(out, vec->data + (i * data_size), data_size);
    return 0;
}

int vec_remove(Vec* vec, void* cmp_against, VecCmpFn* fn) {
    size_t i, len, data_size;

    if (vec->len == 0) {
        return -1;
    }

    len = vec->len;
    data_size = vec->data_size;

    for (i = 0; i < len; ++i) {
        void* c = vec->data + (i * data_size);
        if (fn(cmp_against, c) == 0) {
            goto found;
        }
    }
    return -1;

found:
    memset(vec->data + (i * data_size), 0, data_size);
    if (i == len) {
        vec->len--;
        return 0;
    }
    memmove(vec->data + (i * data_size), vec->data + ((i * data_size) + data_size), data_size * (len - i));
    vec->len--;
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

size_t vec_len(Vec* vec) {
    return vec->len;
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

VecIter vec_iter_new(Vec* vec, int direction) {
    VecIter iter = { 0 };
    ssize_t len = vec->len - 1;
    size_t data_size = vec->data_size;
    if (vec->len == 0) {
        iter.cur = NULL;
        iter.next = NULL;
        iter.next_idx = 1;
        iter.end_idx = len;
        iter.direction = direction;
        iter.vec = vec;
    }

    if (vec->len == 1) {
        iter.cur = vec->data;
        iter.next = NULL;
        iter.next_idx = 1;
        iter.end_idx = len;
        iter.direction = direction;
        iter.vec = vec;
    }

    if (direction == VEC_ITER_REVERSE) {
        size_t start_idx = len * data_size;
        iter.cur = vec->data + start_idx;
        iter.next = vec->data + ((len - 1) * data_size);
        iter.next_idx = len - 1;
        iter.end_idx = 0;
        iter.direction = VEC_ITER_REVERSE;
        iter.vec = vec;
    } else {
        iter.cur = vec->data;
        iter.next = vec->data + data_size;
        iter.next_idx = 1;
        iter.end_idx = len;
        iter.direction = 0;
        iter.vec = vec;
    }

    return iter;
}

void vec_iter_next(VecIter* iter) {
    size_t data_size = iter->vec->data_size;
    if (iter->direction == VEC_ITER_REVERSE) {
        iter->next_idx--;
        iter->cur = iter->next;
        if (iter->next_idx < iter->end_idx) {
            iter->next = NULL;
        } else {
            iter->next = iter->vec->data + (data_size * iter->next_idx);
        }
    } else {
        iter->next_idx++;
        iter->cur = iter->next;
        if (iter->next_idx > iter->end_idx) {
            iter->next = NULL;
        } else {
            iter->next = iter->vec->data + (data_size * iter->next_idx);
        }
    }
}

