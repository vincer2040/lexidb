#include "builder.h"
#include "object.h"
#include "vstr.h"

#define STRING_TYPE_BYTE '$'
#define ARRAY_TYPE_BYTE '*'
#define INT_TYPE_BYTE ':'

static int builder_add_end(builder* b);

builder builder_new() { return vstr_new(); }

const uint8_t* builder_out(builder* b) { return (const uint8_t*)vstr_data(b); }

size_t builder_len(builder* b) { return vstr_len(b); }

int builder_add_ok(builder* b) { return vstr_push_string_len(b, "+OK\r\n", 5); }

int builder_add_ping(builder* b) {
    return vstr_push_string_len(b, "+PING\r\n", 7);
}

int builder_add_pong(builder* b) {
    return vstr_push_string_len(b, "+PONG\r\n", 7);
}

int builder_add_none(builder* b) {
    return vstr_push_string_len(b, "+NONE\r\n", 7);
}

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

int builder_add_int(builder* b, int64_t val) {
    size_t i, len = 8;
    int res = 0;
    uint64_t uval = val;
    uint8_t shift = 56;
    res = vstr_push_char(b, ':');
    if (res == -1) {
        return -1;
    }

    for (i = 0; i < len; ++i, shift -= 8) {
        uint8_t byte = uval >> shift;
        res = vstr_push_char(b, byte);
        if (res == -1) {
            return -1;
        }
    }

    res = builder_add_end(b);
    return res;
}

int builder_add_object(builder* b, object* obj) {
    switch (obj->type) {
    case Null:
        return builder_add_none(b);
    case Int:
        return builder_add_int(b, obj->data.num);
    case String:
        return builder_add_string(b, vstr_data(&(obj->data.string)),
                                  vstr_len(&(obj->data.string)));
    }
    return -1;
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
