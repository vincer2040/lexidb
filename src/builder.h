#ifndef __BUILDER_H__

#define __BUILDER_H__

#include "vstr.h"
#include "object.h"

typedef vstr builder;

builder builder_new(void);
unsigned char* builder_out(const builder* b);
size_t builder_len(const builder* b);
int builder_add_simple_string(builder* b, const char* str, size_t str_len);
int builder_add_bulk_string(builder* b, const char* str, size_t str_len);
int builder_add_int(builder* b, int64_t value);
int builder_add_double(builder* b, double dbl);
int builder_add_array(builder* b, size_t len);
int builder_add_null(builder* b);
int builder_add_boolean(builder* b, int value);
int builder_add_simple_err(builder* b, const char* err, size_t err_len);
int builder_add_bulk_err(builder* b, const char* err, size_t err_len);
int builder_add_object(builder* b, const object* obj);
void builder_free(builder* b);

#endif /* __BUILDER_H__ */
