#include "builder.h"

#define SIMPLE_STRING_BYTE '+'
#define BULK_STRING_BYTE '$'
#define INT_BYTE ':'
#define DBL_BYTE ','
#define ARRAY_BYTE '*'

static int builder_add_len(builder* b, size_t len);
static int builder_add_end(builder* b);

builder builder_new(void) {
    return vstr_new();
}

size_t builder_len(const builder* b) {
    return vstr_len(b);
}

const unsigned char* builder_out(const builder* b) {
    return (const unsigned char*)vstr_data(b);
}

int builder_add_simple_string(builder* b, const char* str, size_t str_len) {
    int res;
    res = vstr_push_char(b, SIMPLE_STRING_BYTE);
    if (res == -1) {
        return -1;
    }
    res = vstr_push_string_len(b, str, str_len);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    return res;
}

int builder_add_bulk_string(builder* b, const char* str, size_t str_len) {
    int res;
    res = vstr_push_char(b, BULK_STRING_BYTE);
    if (res == -1) {
        return -1;
    }
    res = builder_add_len(b, str_len);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    if (res == -1) {
        return -1;
    }
    res = vstr_push_string_len(b, str, str_len);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    return res;
}

int builder_add_int(builder* b, int64_t value) {
    int res;
    vstr value_vstr;
    const char* value_str;
    size_t value_str_len;
    res = vstr_push_char(b, INT_BYTE);
    if (res == -1) {
        return -1;
    }
    value_vstr = vstr_format("%ld", value);
    value_str_len = vstr_len(&value_vstr);
    if (value_str_len == 0) {
        return -1;
    }
    value_str = vstr_data(&value_vstr);
    res = vstr_push_string_len(b, value_str, value_str_len);
    vstr_free(&value_vstr);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    return res;
}

int builder_add_double(builder* b, double dbl) {
    int res;
    vstr dbl_vstr;
    const char* dbl_str;
    size_t dbl_str_len;
    res = vstr_push_char(b, DBL_BYTE);
    if (res == -1) {
        return -1;
    }
    dbl_vstr = vstr_format("%g", dbl);
    dbl_str_len = vstr_len(&dbl_vstr);
    if (dbl_str_len == 0) {
        return -1;
    }
    dbl_str = vstr_data(&dbl_vstr);
    res = vstr_push_string_len(b, dbl_str, dbl_str_len);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    return res;
}

int builder_add_array(builder* b, size_t len) {
    int res;
    res = vstr_push_char(b, ARRAY_BYTE);
    if (res == -1) {
        return -1;
    }
    res = builder_add_len(b, len);
    if (res == -1) {
        return -1;
    }
    res = builder_add_end(b);
    return res;
}

static int builder_add_len(builder* b, size_t len) {
    int res;
    vstr len_vstr;
    const char* len_str;
    size_t len_str_len;
    len_vstr = vstr_format("%lu", len);
    len_str_len = vstr_len(&len_vstr);
    if (len_str_len == 0) {
        return -1;
    }
    len_str = vstr_data(&len_vstr);
    res = vstr_push_string_len(b, len_str, len_str_len);
    vstr_free(&len_vstr);
    return res;
}

static int builder_add_end(builder* b) {
    const char* end = "\r\n";
    return vstr_push_string_len(b, end, 2);
}
