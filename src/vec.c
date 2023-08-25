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

int vec_remove(Vec* vec, void* cmp_against, VecCmpFn* fn) {
    size_t i, len, data_size;

    if (vec->len == 0) {
        return -1;
    }

    len = vec->len - 1;
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

VecIter vec_iter_new(Vec* vec) {
    VecIter iter;
    size_t len;
    size_t data_size;

    len = vec->len - 1;
    data_size = vec->data_size;

    iter.start = vec->data;
    iter.end = vec->data + (len * data_size);

    iter.cur = iter.start;

    iter.next = vec->data + data_size;

    iter.vec = vec;

    return iter;
}

void vec_iter_next(VecIter* iter) {
    iter->at_idx++;
    iter->cur = iter->next;

    if (iter->cur == iter->end) {
        iter->next = NULL;
    } else {
        size_t i = iter->at_idx * iter->vec->data_size;
        iter->next = iter->vec->data + i;
    }
}

