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

Builder create_builder(size_t initial_cap);
void free_builder(Builder* builder);
int builder_add_arr(Builder* builder, size_t arr_len);
int builder_add_string(Builder* builder, char* str, size_t str_len);

#endif
