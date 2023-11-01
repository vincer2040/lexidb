#ifndef __BUILDER_H__

#define __BUILDER_H__

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t ins;
    size_t cap;
    uint8_t* buf;
} Builder;

#define dbgbuf(builder)                                                        \
    {                                                                          \
        size_t i;                                                              \
        for (i = 0; i < builder.ins; ++i) {                                    \
            printf("%x ", builder.buf[i]);                                     \
        }                                                                      \
        printf("\n");                                                          \
    }

Builder builder_create(size_t initial_cap);
void builder_free(Builder* builder);
int builder_add_pong(Builder* builder);
int builder_add_ping(Builder* builder);
int builder_add_ok(Builder* builder);
int builder_add_none(Builder* builder);
int builder_add_err(Builder* builder, uint8_t* e, size_t e_len);
int builder_add_arr(Builder* builder, size_t arr_len);
int builder_add_string(Builder* builder, char* str, size_t str_len);
int builder_add_int(Builder* builder, int64_t val);
uint8_t* builder_out(Builder* builder);
void builder_reset(Builder* builder);
int builder_copy_from(Builder* builder, uint8_t* source, size_t needed_len);

#endif
