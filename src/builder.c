#include "builder.h"
#include "object.h"
#include "vstr.h"
#include <stdio.h>

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

    vstr_free(&len_vstr);

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
    vstr_free(&len_vstr);
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
    int res = 0;
    vstr int_str = vstr_format("%ld\n", val);
    const char* int_str_s = vstr_data(&int_str);
    size_t int_str_len = vstr_len(&int_str);
    res = vstr_push_char(b, ':');
    if (res == -1) {
        vstr_free(&int_str);
        return -1;
    }

    res = vstr_push_string_len(b, int_str_s, int_str_len);
    vstr_free(&int_str);
    if (res == -1) {
        return -1;
    }

    res = builder_add_end(b);
    return res;
}

int builder_add_double(builder* b, double val) {
    vstr tmp = vstr_format("%f", val);
    const char* tmp_data = vstr_data(&tmp);
    size_t tmp_len = vstr_len(&tmp);
    int res = vstr_push_char(b, ',');
    if (res == -1) {
        vstr_free(&tmp);
        return -1;
    }
    res = vstr_push_string_len(b, tmp_data, tmp_len);
    if (res == -1) {
        vstr_free(&tmp);
        return -1;
    }
    vstr_free(&tmp);
    res = builder_add_end(b);
    return res;
}

int builder_add_ht(builder* b, size_t len) {
    int add_res;
    vstr len_str = vstr_format("%lu", len);
    const char* len_str_s = vstr_data(&len_str);
    size_t len_str_len = vstr_len(&len_str);
    add_res = vstr_push_char(b, '%');
    if (add_res == -1) {
        vstr_free(&len_str);
        return -1;
    }
    add_res = vstr_push_string_len(b, len_str_s, len_str_len);
    if (add_res == -1) {
        vstr_free(&len_str);
        return -1;
    }
    vstr_free(&len_str);
    return builder_add_end(b);
}

int builder_add_object(builder* b, object* obj) {
    switch (obj->type) {
    case Null:
        return builder_add_none(b);
    case Int:
        return builder_add_int(b, obj->data.num);
    case Double:
        return builder_add_double(b, obj->data.dbl);
    case String:
        return builder_add_string(b, vstr_data(&(obj->data.string)),
                                  vstr_len(&(obj->data.string)));
    case Array: {
        vec_iter iter = vec_iter_new(obj->data.vec);
        int add_res = builder_add_array(b, obj->data.vec->len);
        if (add_res == -1) {
            return -1;
        }
        while (iter.cur) {
            object* cur = iter.cur;
            add_res = builder_add_object(b, cur);
            if (add_res == -1) {
                return -1;
            }
            vec_iter_next(&iter);
        }
        return 0;
    }
    case Ht: {
        ht_iter iter = ht_iter_new(&obj->data.ht);
        int add_res = builder_add_ht(b, obj->data.ht.num_entries);
        if (add_res == -1) {
            return -1;
        }
        while (iter.cur) {
            ht_entry* cur = iter.cur;
            object* key = (object*)ht_entry_get_key(cur);
            object* value = (object*)ht_entry_get_value(cur);
            add_res = builder_add_object(b, key);
            if (add_res == -1) {
                return -1;
            }
            add_res = builder_add_object(b, value);
            if (add_res == -1) {
                return -1;
            }
        }
        return 0;
    }
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
