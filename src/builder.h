#ifndef __BUILDER_H__

#define __BUILDER_H__

#include "vstr.h"

typedef vstr builder;

builder builder_new(void);
const unsigned char* builder_out(const builder* b);
size_t builder_len(const builder* b);
int builder_add_simple_string(builder* b, const char* str, size_t str_len);
int builder_add_bulk_string(builder* b, const char* str, size_t str_len);
int builder_add_int(builder* b, int64_t value);
int builder_add_double(builder* b, double dbl);
int builder_add_array(builder* b, size_t len);

#endif /* __BUILDER_H__ */
