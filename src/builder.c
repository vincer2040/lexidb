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

static int builder_realloc(Builder* builder, size_t needed) {
    size_t new_cap = builder->cap + needed;
    void* tmp = realloc(builder->buf, new_cap);
    if (tmp == NULL) {
        return -1;
    }
    builder->buf = tmp;
    builder->cap = new_cap;
    memset(builder->buf + builder->ins, 0, new_cap - builder->ins);
    return 0;
}

static inline void builder_add_type_byte(Builder* builder, uint8_t byte) {
    builder->buf[builder->ins] = byte;
    builder->ins += 1;
}

static inline void builder_add_byte(Builder* builder, uint8_t byte) {
    builder->buf[builder->ins] = byte;
    builder->ins += 1;
}

static inline void builder_add_str(Builder* builder, uint8_t* str, size_t len) {
    memcpy(builder->buf + builder->ins, str, len);
    builder->ins += len;
}

static inline void builder_add_end(Builder* builder) {
    builder->buf[builder->ins] = '\r';
    builder->ins += 1;
    builder->buf[builder->ins] = '\n';
    builder->ins += 1;
}

static inline void builder_add_len(Builder* builder, size_t len) {
    size_t len_buf_len;
    char len_buf[20] = {0};
    sprintf(len_buf, "%lu", len);
    len_buf_len = strlen(len_buf);
    memcpy(builder->buf + builder->ins, len_buf, len_buf_len);
    builder->ins += len_buf_len;
}

int builder_add_pong(Builder* builder) {
    size_t needed = builder->ins + 7;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '+');
    builder_add_str(builder, (uint8_t*)"PONG", 4);
    builder_add_end(builder);
    return 0;
}

int builder_add_ping(Builder* builder) {
    size_t needed = builder->ins + 7;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '+');
    builder_add_str(builder, (uint8_t*)"PING", 4);
    builder_add_end(builder);
    return 0;
}

int builder_add_ok(Builder* builder) {
    size_t needed = builder->ins + 5;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '+');
    builder_add_str(builder, (uint8_t*)"OK", 2);
    builder_add_end(builder);
    return 0;
}

int builder_add_none(Builder* builder) {
    size_t needed = builder->ins + 7;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '+');
    builder_add_str(builder, (uint8_t*)"NONE", 4);
    builder_add_end(builder);
    return 0;
}

int builder_add_err(Builder* builder, uint8_t* e, size_t e_len) {
    size_t needed = builder->ins + 3 + e_len;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '-');
    builder_add_str(builder, e, e_len);
    builder_add_end(builder);
    return 0;
}

int builder_add_arr(Builder* builder, size_t arr_len) {
    size_t needed = builder->ins + 21;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '*');
    builder_add_len(builder, arr_len);
    builder_add_end(builder);
    return 0;
}

int builder_add_string(Builder* builder, char* str, size_t str_len) {
    size_t needed = builder->ins + str_len + 20 + 5;
    size_t cap = builder->cap;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, '$');
    builder_add_len(builder, str_len);
    builder_add_end(builder);
    builder_add_str(builder, (uint8_t*)str, str_len);
    builder_add_end(builder);
    return 0;
}

int builder_add_int(Builder* builder, int64_t val) {
    size_t needed = builder->ins + 11;
    size_t cap = builder->cap;
    size_t i, len = 8, shift = 56;
    if (cap < needed) {
        needed <<= 1;
        if (builder_realloc(builder, needed) == -1) {
            return -1;
        }
    }
    builder_add_type_byte(builder, ':');
    for (i = 0; i < len; ++i, shift -= 8) {
        uint8_t at = val >> shift;
        builder_add_byte(builder, at);
    }
    builder_add_end(builder);
    return 0;
}

void builder_reset(Builder* builder) {
    memset(builder->buf, 0, builder->ins);
    builder->ins = 0;
}

uint8_t* builder_out(Builder* builder) { return builder->buf; }

void builder_free(Builder* builder) {
    free(builder->buf);
    builder->cap = 0;
    builder->ins = 0;
}
