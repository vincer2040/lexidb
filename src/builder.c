#include "builder.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Builder create_builder(size_t initial_cap) {
    Builder builder = {0};

    builder.cap = initial_cap;
    builder.ins = 0;
    builder.buf = calloc(initial_cap, sizeof builder.buf);
    assert(builder.buf != NULL);

    return builder;
}

int builder_add_arr(Builder* builder, size_t arr_len) {
    size_t needed_len;
    size_t str_len_buf_len;
    char str_len_buf[20] = {0};
    sprintf(str_len_buf, "%lu", arr_len);
    str_len_buf_len = strlen(str_len_buf);

    needed_len = builder->ins + str_len_buf_len + 3;

    if (needed_len > builder->cap) {
        builder->cap = needed_len + 120;
        builder->buf = realloc(builder->buf, (sizeof(uint8_t)) * builder->cap);
        if (builder->buf == NULL) {
            builder->cap = 0;
            builder->ins = 0;
            return -1;
        }
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
        builder->cap = needed_len + 120;
        builder->buf = realloc(builder->buf, (sizeof(uint8_t)) * builder->cap);
        if (builder->buf == NULL) {
            builder->cap = 0;
            builder->ins = 0;
            return -1;
        }
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
    memcpy(builder->buf + builder->ins, str, str_len);
    builder->ins += str_len;
    builder->buf[builder->ins] = '\r';
    builder->ins++;
    builder->buf[builder->ins] = '\n';
    builder->ins++;
    return 0;
}

uint8_t* builder_out(Builder* builder) { return builder->buf; }

void free_builder(Builder* builder) {
    free(builder->buf);
    builder->cap = 0;
    builder->ins = 0;
}
