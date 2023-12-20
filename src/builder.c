#include "builder.h"
#include "vstr.h"

#define STRING_TYPE_BYTE '$'
#define ARRAY_TYPE_BYTE '*'
#define INT_TYPE_BYTE ':'

static int builder_add_end(builder* b);

builder builder_new() { return vstr_new(); }

const uint8_t* builder_out(builder* b) { return (const uint8_t*)vstr_data(b); }

size_t builder_len(builder* b) { return vstr_len(b); }

int builder_add_ok(builder* b) { return vstr_push_string(b, "+OK\r\n"); }

int builder_add_pong(builder* b) { return vstr_push_string(b, "+PONG\r\n"); }

int builder_add_array(builder* b, size_t arr_len) {
    int res;
    vstr len_vstr;
    const char* len_str;
    size_t len_str_len;

    len_vstr = vstr_format("%lu", arr_len);
    len_str_len = vstr_len(&len_vstr);
    len_str = vstr_data(&len_vstr);

    if (len_str == 0) {
        return -1;
    }

    res = vstr_push_char(b, ARRAY_TYPE_BYTE);
    if (res == -1) {
        return -1;
    }

    res = vstr_push_string_len(b, len_str, len_str_len);
    if (res == -1) {
        return -1;
    }

    res = builder_add_end(b);
    return res;
}

int builder_add_err(builder* b, const char* str, size_t len) {
    int res = 0;

    res = vstr_push_char(b, '-');
    if (res == -1) {
        return -1;
    }

    res = vstr_push_string_len(b, str, len);
    if (res == -1) {
        return -1;
    }

    res = builder_add_end(b);

    return res;
}

int builder_add_string(builder* b, const char* str, size_t str_len) {
    vstr len_vstr;
    const char* len_str;
    size_t len_str_len;
    int res;

    res = vstr_push_char(b, STRING_TYPE_BYTE);

    if (res == -1) {
        return -1;
    }

    len_vstr = vstr_format("%lu", str_len);
    len_str = vstr_data(&len_vstr);
    len_str_len = vstr_len(&len_vstr);
    res = vstr_push_string_len(b, len_str, len_str_len);
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

void builder_reset(builder* b) { vstr_reset(b); }

void builder_free(builder* b) { vstr_free(b); }

static int builder_add_end(builder* b) {
    int res = 0;
    res = vstr_push_char(b, '\r');
    if (res == -1) {
        return -1;
    }
    res = vstr_push_char(b, '\n');
    if (res == -1) {
        return -1;
    }
    return res;
}
