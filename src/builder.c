#include "builder.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builder builder_create(size_t initial_cap) {
    Builder builder = {0};

    builder.cap = initial_cap;
    builder.ins = 0;
    builder.buf = calloc(initial_cap, sizeof builder.buf);

    assert(builder.buf != NULL);

    return builder;
}

int builder_add_pong(Builder* builder) {
    if (builder->cap < 7) {
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * 7);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf, 0, 7);
    }
    memcpy(builder->buf, "+PONG\r\n", 7);
    builder->ins = 7;
    return 0;
}

int builder_add_ping(Builder* builder) {
    if (builder->cap < 7) {
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * 7);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf, 0, 7);
    }
    memcpy(builder->buf, "+PING\r\n", 7);
    builder->ins = 7;
    return 0;
}

int builder_add_ok(Builder* builder) {
    if (builder->cap < 5) {
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * 5);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf, 0, 5);
    }
    memcpy(builder->buf, "+OK\r\n", 5);
    builder->ins = 5;
    return 0;
}

int builder_add_none(Builder* builder) {
    if (builder->cap < 7) {
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * 7);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf, 0, 7);
    }
    memcpy(builder->buf, "+NONE\r\n", 7);
    builder->ins = 7;
    return 0;
}

int builder_add_err(Builder* builder, uint8_t* e, size_t e_len) {
    size_t needed_len = 3 + e_len;
    if (builder->cap < needed_len) {
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * needed_len);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf, 0, needed_len);
    }
    builder->buf[0] = ((uint8_t)'-');
    memcpy(builder->buf + 1, e, e_len);
    builder->buf[e_len + 1] = '\r';
    builder->buf[e_len + 2] = '\n';
    builder->ins = needed_len;
    return 0;
}

int builder_add_arr(Builder* builder, size_t arr_len) {
    size_t needed_len;
    size_t str_len_buf_len;
    char str_len_buf[20] = {0};
    sprintf(str_len_buf, "%lu", arr_len);
    str_len_buf_len = strlen(str_len_buf);

    needed_len = builder->ins + str_len_buf_len + 3;

    if (needed_len > builder->cap) {
        void* tmp;
        builder->cap = needed_len + 120;
        tmp = realloc(builder->buf, (sizeof(uint8_t)) * builder->cap);
        if (tmp == NULL) {
            builder->cap = 0;
            builder->ins = 0;
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf + builder->ins, 0, builder->cap - builder->ins);
    }
    builder->buf[builder->ins] = '*';
    builder->ins++;
    memcpy(builder->buf + builder->ins, str_len_buf, str_len_buf_len);
    builder->ins += str_len_buf_len;
    builder->buf[builder->ins] = '\r';
    builder->ins++;
    builder->buf[builder->ins] = '\n';
    builder->ins++;
    return 0;
}

int builder_add_string(Builder* builder, char* str, size_t str_len) {
    size_t needed_len;
    size_t str_len_buf_len;
    char str_len_buf[20] = {0};
    sprintf(str_len_buf, "%lu", str_len);
    str_len_buf_len = strlen(str_len_buf);

    needed_len = builder->ins + str_len_buf_len + 5 + str_len;

    if (needed_len > builder->cap) {
        void* tmp;
        builder->cap = needed_len + 120;
        tmp = realloc(builder->buf, (sizeof(uint8_t)) * builder->cap);
        if (tmp == NULL) {
            builder->cap = 0;
            builder->ins = 0;
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf + builder->ins, 0, builder->cap - builder->ins);
    }
    builder->buf[builder->ins] = '$';
    builder->ins++;
    memcpy(builder->buf + builder->ins, str_len_buf, str_len_buf_len);
    builder->ins += str_len_buf_len;
    builder->buf[builder->ins] = '\r';
    builder->ins++;
    builder->buf[builder->ins] = '\n';
    builder->ins++;
    memcpy(builder->buf + builder->ins, str, str_len);
    builder->ins += str_len;
    builder->buf[builder->ins] = '\r';
    builder->ins++;
    builder->buf[builder->ins] = '\n';
    builder->ins++;
    return 0;
}

int builder_add_int(Builder* builder, int64_t val) {
    size_t needed_len = builder->ins + sizeof(int64_t) + 3;
    size_t cap = builder->cap;
    size_t len = builder->ins;
    uint8_t shift = 56;
    size_t i;

    if (needed_len > cap) {
        cap += needed_len + 32;
        void* tmp;
        tmp = realloc(builder->buf, sizeof(uint8_t) * cap);
        if (tmp == NULL) {
            return -1;
        }
        builder->buf = tmp;
        memset(builder->buf + len, 0, cap - len);
        builder->cap = cap;
    }

    builder->buf[len] = ':';
    len++;

    for (i = 0; i < 8; ++i, ++len, shift-=8) {
        builder->buf[len] = val >> shift;
    }

    builder->buf[len] = '\r';
    len++;
    builder->buf[len] = '\n';

    builder->ins += 11;
    return 0;
}

uint8_t* builder_out(Builder* builder) { return builder->buf; }

void builder_free(Builder* builder) {
    free(builder->buf);
    builder->cap = 0;
    builder->ins = 0;
}
