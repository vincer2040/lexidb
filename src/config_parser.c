#include "config_parser.h"
#include "ht.h"
#include "object.h"
#include "vstr.h"
#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const char* input;
    size_t input_len;
    size_t pos;
    char ch;
} config_parser;

typedef struct {
    line_data_type type;
    object data;
} line_data;

typedef struct {
    const char* str;
    size_t str_len;
    line_data_type type;
} line_data_type_lookup;

result_t(line_data, vstr);
result_t(line_data_type, vstr);

const line_data_type_lookup lookups[] = {
    {"port", 4, Port},
    {"user", 4, User},
    {"address", 8, Address},
    {"databases", 9, Databases},
};

const size_t lookups_len = sizeof lookups / sizeof lookups[0];

static config_parser config_parser_new(const char* input, size_t input_len);
static result(ht) parse(config_parser* p);
static result(line_data) parse_line(config_parser* p);
static object parse_user(config_parser* p);
static object parse_number_obj(config_parser* p);
static result(line_data_type) parse_line_data_type(config_parser* p);
static vstr parse_string(config_parser* p);
static int64_t parse_number(config_parser* p);
static inline bool cur_char_is(config_parser* p, char ch);
static void skip_line(config_parser* p);
static void config_parser_read_char(config_parser* p);
static void config_free_object_in_ht(void* ptr);

result(ht) parse_config(const char* input, size_t input_len) {
    config_parser p = config_parser_new(input, input_len);
    return parse(&p);
}

void config_free(ht* config) {
    ht_free(config, NULL, config_free_object_in_ht);
}

static config_parser config_parser_new(const char* input, size_t input_len) {
    config_parser p = {input, input_len, 0, 0};
    config_parser_read_char(&p);
    return p;
}

static result(ht) parse(config_parser* p) {
    result(ht) res = {0};
    ht ht = ht_new(sizeof(object), NULL);
    while (p->ch != 0) {
        result(line_data) data_res;
        line_data data;
        int insert_res = 0;
        if (cur_char_is(p, '\n')) {
            goto next_line;
        }
        /* this line is a comment */
        if (cur_char_is(p, '#')) {
            skip_line(p);
            goto next_line;
        }
        data_res = parse_line(p);
        if (data_res.type == Err) {
            config_free(&ht);
            res.type = Err;
            res.data.err = data_res.data.err;
            return res;
        }

        data = data_res.data.ok;

        switch (data.type) {
        case Address:
            break;
        case Port:
            break;
        case User: {
            object* cur = ht_get(&ht, &data.type, sizeof(line_data_type));
            if (cur == NULL) {
                object user_vec_obj;
                vec* user_vec = vec_new(sizeof(object));
                assert(user_vec != NULL);
                vec_push(&user_vec, &data.data);
                user_vec_obj = object_new(Array, &user_vec);
                insert_res = ht_insert(&ht, &data.type, sizeof(line_data_type),
                                       &user_vec_obj, NULL, NULL);
            } else {
                assert(cur->type == Array);
                vec_push(&cur->data.vec, &data.data);
            }
        } break;
        case Databases:
            insert_res = ht_try_insert(&ht, &data.type, sizeof(line_data_type),
                                       &data.data);
            break;
        }

        // insert_res = ht_try_insert(&ht, &data_res.data.ok.type,
        // sizeof(line_data_type), &data_res.data.ok.data);
        if (insert_res != HT_OK) {
            res.type = Err;
            res.data.err = vstr_from("invalid duplicate type in config");
            config_free(&ht);
            object_free(&data_res.data.ok.data);
            return res;
        }
    next_line:
        config_parser_read_char(p);
    }
    res.type = Ok;
    res.data.ok = ht;
    return res;
}

static result(line_data) parse_line(config_parser* p) {
    result(line_data) res = {0};
    result(line_data_type) data_type_res = parse_line_data_type(p);
    if (data_type_res.type == Err) {
        res.type = Err;
        res.data.err = data_type_res.data.err;
        return res;
    }
    config_parser_read_char(p);
    switch (data_type_res.data.ok) {
    case Address:
        res.type = Err;
        res.data.err = vstr_from("address not yet implemented");
        break;
    case Port:
        res.type = Err;
        res.data.err = vstr_from("port not yet implemented");
        break;
    case User:
        res.type = Ok;
        res.data.ok.type = User;
        res.data.ok.data = parse_user(p);
        break;
    case Databases:
        res.type = Ok;
        res.data.ok.type = Databases;
        res.data.ok.data = parse_number_obj(p);
    }
    return res;
}

static object parse_user(config_parser* p) {
    object res, username_obj, password_obj;
    vstr username, password;
    vec* user_data;
    username = parse_string(p);
    config_parser_read_char(p);
    password = parse_string(p);
    user_data = vec_new_with_capacity(sizeof(object), 2);
    username_obj = object_new(String, &username);
    password_obj = object_new(String, &password);
    vec_push(&user_data, &username_obj);
    vec_push(&user_data, &password_obj);
    res = object_new(Array, &user_data);
    return res;
}

static object parse_number_obj(config_parser* p) {
    object res = {0};
    res.type = Int;
    res.data.num = parse_number(p);
    return res;
}

static result(line_data_type) parse_line_data_type(config_parser* p) {
    result(line_data_type) res = {0};
    vstr type_vstr = parse_string(p);
    const char* type_string = vstr_data(&type_vstr);
    size_t type_string_len = vstr_len(&type_vstr);
    size_t i;
    for (i = 0; i < lookups_len; ++i) {
        line_data_type_lookup l = lookups[i];
        if (l.str_len != type_string_len) {
            continue;
        }
        if (memcmp(l.str, type_string, type_string_len) != 0) {
            continue;
        }
        res.type = Ok;
        res.data.ok = l.type;
        return res;
    }
    res.type = Err;
    res.data.err =
        vstr_format("invalid data type %s in config file", type_string);
    return res;
}

static vstr parse_string(config_parser* p) {
    vstr s = vstr_new();
    while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    return s;
}

static int64_t parse_number(config_parser* p) {
    int64_t res = 0;
    while (isdigit(p->ch)) {
        res = (res * 10) + (p->ch - '0');
        config_parser_read_char(p);
    }
    return res;
}

static void skip_line(config_parser* p) {
    while (p->ch != '\n' && p->ch != 0) {
        config_parser_read_char(p);
    }
}

static inline bool cur_char_is(config_parser* p, char ch) {
    return p->ch == ch;
}

static void config_parser_read_char(config_parser* p) {
    if (p->pos >= p->input_len) {
        p->ch = 0;
        return;
    }
    p->ch = p->input[p->pos];
    p->pos++;
}

static void config_free_object_in_ht(void* ptr) {
    object* obj = ptr;
    object_free(obj);
}
