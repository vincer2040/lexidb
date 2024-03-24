#define _XOPEN_SOURCE 600
#include "config_parser.h"
#include "ht.h"
#include "object.h"
#include "server.h"
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
    union {
        vstr address;
        uint16_t port;
        size_t databases;
        user user;
    } data;
} line_data;

typedef struct {
    const char* str;
    size_t str_len;
    line_data_type type;
} line_data_type_lookup;

result_t(line_data, vstr);
result_t(line_data_type, vstr);
result_t(user, vstr);
result_t(size_t, vstr);
result_t(uint16_t, vstr);

const line_data_type_lookup lookups[] = {
    {"port", 4, Port},
    {"user", 4, User},
    {"address", 7, Address},
    {"databases", 9, Databases},
};

const size_t lookups_len = sizeof lookups / sizeof lookups[0];

static config_parser config_parser_new(const char* input, size_t input_len);
static config config_new(void);
static result(config) config_parser_parse(config_parser* p);
static result(line_data) config_parser_parse_line(config_parser* p);
static result(line_data_type)
    config_parser_parse_line_data_type(config_parser* p);
static result(user) config_parser_parse_user(config_parser* p);
static result(size_t) config_parser_parse_num_databases(config_parser* p);
static result(uint16_t) config_parser_parse_port(config_parser* p);
static vstr config_parser_parse_address(config_parser* p);
static vstr config_parser_read_string(config_parser* p);
static void config_parser_read_char(config_parser* p);
static void config_parser_skip_empty_lines_and_comments(config_parser* p);
static void config_parser_skip_whitespace(config_parser* p);
static void config_parser_skip_spaces(config_parser* p);
static void config_parser_skip_comment(config_parser* p);
static void config_free_user(void* ptr);

result(config) parse_config(const char* input, size_t input_len) {
    config_parser p = config_parser_new(input, input_len);
    return config_parser_parse(&p);
}

void config_free(config* config) { vec_free(config->users, config_free_user); }

static config_parser config_parser_new(const char* input, size_t input_len) {
    config_parser p = {0};
    p.input = input;
    p.input_len = input_len;
    p.ch = 0;
    p.pos = 0;
    config_parser_read_char(&p);
    return p;
}

static config config_new(void) {
    config c = {0};
    c.users = vec_new(sizeof(user));
    assert(c.users != NULL);
    c.address = vstr_new();
    return c;
}

static result(config) config_parser_parse(config_parser* p) {
    result(config) res = {0};
    config config = config_new();
    while (p->ch != 0) {
        result(line_data) line_data_res;
        line_data line_data;
        config_parser_skip_empty_lines_and_comments(p);
        if (p->ch == 0) {
            break;
        }
        line_data_res = config_parser_parse_line(p);
        if (line_data_res.type == Err) {
            config_free(&config);
            res.type = Err;
            res.data.err = line_data_res.data.err;
            return res;
        }
        line_data = line_data_res.data.ok;
        switch (line_data.type) {
        case Address:
            if (vstr_len(&config.address) != 0) {
                config_free(&config);
                res.type = Err;
                res.data.err = vstr_from("address set twice in config");
                return res;
            }
            config.address = line_data.data.address;
            break;
        case Port:
            if (config.port != 0) {
                config_free(&config);
                res.type = Err;
                res.data.err = vstr_from("port set twice in config");
                return res;
            }
            config.port = line_data.data.port;
            break;
        case User:
            vec_push(&config.users, &line_data.data.user);
            break;
        case Databases:
            if (config.databases != 0) {
                config_free(&config);
                res.type = Err;
                res.data.err =
                    vstr_from("number of databases set twice in config");
                return res;
            }
            config.databases = line_data.data.databases;
            break;
        }
    }
    res.type = Ok;
    res.data.ok = config;
    return res;
}

static result(line_data) config_parser_parse_line(config_parser* p) {
    result(line_data) res = {0};
    result(line_data_type) type_res = config_parser_parse_line_data_type(p);
    if (type_res.type == Err) {
        res.type = Err;
        res.data.err = type_res.data.err;
        return res;
    }
    switch (type_res.data.ok) {
    case Address: {
        vstr addr;
        config_parser_skip_spaces(p);
        addr = config_parser_parse_address(p);
        res.type = Ok;
        res.data.ok.type = Address;
        res.data.ok.data.address = addr;
    } break;
    case Port: {
        result(uint16_t) p_res;
        config_parser_skip_spaces(p);
        p_res = config_parser_parse_port(p);
        if (p_res.type == Err) {
            res.type = Err;
            res.data.err = p_res.data.err;
            break;
        }
        res.type = Ok;
        res.data.ok.type = Port;
        res.data.ok.data.port = p_res.data.ok;
    } break;
    case User: {
        result(user) u_res;
        config_parser_skip_spaces(p);
        u_res = config_parser_parse_user(p);
        if (u_res.type == Err) {
            res.type = Err;
            res.data.err = u_res.data.err;
            break;
        }
        res.type = Ok;
        res.data.ok.type = User;
        res.data.ok.data.user = u_res.data.ok;
    } break;
    case Databases: {
        result(size_t) databases_res;
        config_parser_skip_spaces(p);
        databases_res = config_parser_parse_num_databases(p);
        if (databases_res.type == Err) {
            res.type = Err;
            res.data.err = databases_res.data.err;
            break;
        }
        res.type = Ok;
        res.data.ok.type = Databases;
        res.data.ok.data.databases = databases_res.data.ok;
    } break;
    }
    return res;
}

static result(line_data_type)
    config_parser_parse_line_data_type(config_parser* p) {
    result(line_data_type) res = {0};
    vstr line_data_type_string = config_parser_read_string(p);
    size_t slen = vstr_len(&line_data_type_string);
    const char* s = vstr_data(&line_data_type_string);
    size_t i;

    for (i = 0; i < lookups_len; ++i) {
        line_data_type_lookup lookup = lookups[i];
        if (lookup.str_len != slen) {
            continue;
        }
        if (memcmp(lookup.str, s, slen) != 0) {
            continue;
        }
        vstr_free(&line_data_type_string);
        res.type = Ok;
        res.data.ok = lookup.type;
        return res;
    }
    res.type = Err;
    res.data.err =
        vstr_format("unknown data type: %s", vstr_data(&line_data_type_string));
    vstr_free(&line_data_type_string);
    return res;
}

static result(user) config_parser_parse_user(config_parser* p) {
    result(user) res = {0};
    user user = {0};
    user.name = config_parser_read_string(p);
    user.password = vstr_new();
    config_parser_skip_spaces(p);
    while (p->ch != '\n' && p->ch != 0) {
        switch (p->ch) {
        case '>':
            config_parser_read_char(p);
            user.password = config_parser_read_string(p);
            break;
        default:
            config_free_user(&user);
            res.type = Err;
            res.data.err = vstr_format(
                "unexpected character when parsing user '%c'", p->ch);
            return res;
        }
        config_parser_skip_spaces(p);
    }
    res.type = Ok;
    res.data.ok = user;
    return res;
}

static result(size_t) config_parser_parse_num_databases(config_parser* p) {
    result(size_t) res = {0};
    size_t num = 0;
    while (isdigit(p->ch)) {
        num = (num * 10) + (p->ch - '0');
        config_parser_read_char(p);
    }
    config_parser_skip_spaces(p);
    if (p->ch != '\n' && p->ch != 0) {
        res.type = Err;
        res.data.err = vstr_from("expected only the number of databases");
    } else {
        res.type = Ok;
        res.data.ok = num;
    }
    return res;
}

static result(uint16_t) config_parser_parse_port(config_parser* p) {
    result(uint16_t) res = {0};
    uint64_t num = 0;
    while (isdigit(p->ch)) {
        num = (num * 10) + (p->ch - '0');
        config_parser_read_char(p);
    }
    config_parser_skip_spaces(p);
    if (p->ch != '\n' && p->ch != 0) {
        res.type = Err;
        res.data.err = vstr_from("expected only the number of databases");
        return res;
    }
    if (num > UINT16_MAX) {
        res.type = Err;
        res.data.err = vstr_format("%lu is too big to be a port", num);
        return res;
    }
    res.type = Ok;
    res.data.ok = num;
    return res;
}

static vstr config_parser_parse_address(config_parser* p) {
    vstr res = vstr_new();
    while (isdigit(p->ch) || p->ch == '.') {
        vstr_push_char(&res, p->ch);
        config_parser_read_char(p);
    }
    config_parser_skip_spaces(p);
    if (p->ch != '\n' && p->ch != 0) {
        vstr_free(&res);
        return res;
    }
    return res;
}

static vstr config_parser_read_string(config_parser* p) {
    vstr s = vstr_new();
    while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    return s;
}

static void config_parser_read_char(config_parser* p) {
    if (p->pos >= p->input_len) {
        p->ch = 0;
        return;
    }
    p->ch = p->input[p->pos];
    p->pos++;
}

static void config_parser_skip_empty_lines_and_comments(config_parser* p) {
    while (true) {
        if (p->ch == '#') {
            config_parser_skip_comment(p);
        } else if (p->ch == '\n') {
            config_parser_skip_whitespace(p);
        } else {
            break;
        }
    }
}

static void config_parser_skip_whitespace(config_parser* p) {
    while (p->ch == ' ' || p->ch == '\t' || p->ch == '\r' || p->ch == '\n') {
        config_parser_read_char(p);
    }
}

static void config_parser_skip_spaces(config_parser* p) {
    while (p->ch == ' ' || p->ch == '\t') {
        config_parser_read_char(p);
    }
}

static void config_parser_skip_comment(config_parser* p) {
    while (p->ch != '\n' && p->ch != 0) {
        config_parser_read_char(p);
    }
}

static void config_free_user(void* ptr) {
    user* user = ptr;
    vstr_free(&user->name);
    vstr_free(&user->password);
}
