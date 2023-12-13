#include "parser.h"
#include "cmd.h"
#include "object.h"
#include <ctype.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>

const int parser_debug = 1;

#define dbgf(...)                                                              \
    if (parser_debug) {                                                        \
        printf("%s:%d ", __FILE__, __LINE__);                                  \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    }

typedef struct {
    const uint8_t* input;
    size_t input_len;
    size_t pos;
    uint8_t ch;
} parser;

typedef struct {
    const char* cmd;
    size_t cmd_len;
    cmdt type;
} cmdt_lookup;

const cmdt_lookup lookup[] = {
    {"SET", 3, Set},
    {"GET", 3, Get},
    {"DEL", 3, Del},
    {"PING", 4, Ping},
};

const size_t lookup_len = sizeof lookup / sizeof lookup[0];

static parser parser_new(const uint8_t* input, size_t input_len);
static cmd parse_cmd(parser* p);
static cmd parse_array_cmd(parser* p);
static cmdt parse_cmd_type(parser* p);
static cmdt lookup_cmd(vstr* s);
static cmdt parse_simple_string_cmd(parser* p);
static object parse_object(parser* p);
static vstr parse_string(parser* p, uint64_t len);
static vstr parse_simple_string(parser* p);
static int64_t parse_integer(parser* p);
static uint64_t parse_len(parser* p);
static uint8_t peek_byte(parser* p);
static bool inline cur_byte_is(parser* p, uint8_t byte);
static bool peek_byte_is(parser* p, uint8_t byte);
static bool expect_peek_byte(parser* p, uint8_t byte);
static bool expect_peek_byte_to_be_num(parser* p);
static inline void parser_read_char(parser* p);

cmd parse(const uint8_t* input, size_t input_len) {
    parser p = parser_new(input, input_len);
    return parse_cmd(&p);
}

static parser parser_new(const uint8_t* input, size_t input_len) {
    parser p = {0};
    p.input = input;
    p.input_len = input_len;
    parser_read_char(&p);
    return p;
}

static cmd parse_cmd(parser* p) {
    cmd cmd = {0};
    switch (p->ch) {
    case '*':
        cmd = parse_array_cmd(p);
        break;
    case '+':
        cmd.type = parse_simple_string_cmd(p);
        break;
    default:
        break;
    }
    return cmd;
}

static cmd parse_array_cmd(parser* p) {
    cmd cmd = {0};
    uint64_t len;
    cmdt type;

    if (!expect_peek_byte_to_be_num(p)) {
        return cmd;
    }

    len = parse_len(p);

    if (!cur_byte_is(p, '\r')) {
        return cmd;
    }
    if (!expect_peek_byte(p, '\n')) {
        return cmd;
    }

    if (len == 0) {
        return cmd;
    }

    parser_read_char(p);

    type = parse_cmd_type(p);

    if (type == Illegal) {
        return cmd;
    }

    switch (type) {
    case Set: {
        object key;
        object value;
        set_cmd set = {0};
        if (len != 3) {
            return cmd;
        }
        key = parse_object(p);
        if (key.type == Null) {
            return cmd;
        }
        value = parse_object(p);
        set.key = key;
        set.value = value;
        cmd.type = Set;
        cmd.data.set = set;
    } break;
    case Get: {
        object key;
        get_cmd get = {0};
        if (len != 2) {
            return cmd;
        }
        key = parse_object(p);
        if (key.type == Null) {
            return cmd;
        }
        get.key = key;
        cmd.type = Get;
        cmd.data.get = get;
    } break;
    case Del: {
        object key;
        del_cmd del = {0};
        if (len != 2) {
            return cmd;
        }
        key = parse_object(p);
        if (key.type == Null) {
            return cmd;
        }
        del.key = key;
        cmd.type = Del;
        cmd.data.del = del;
    } break;
    default:
        break;
    }

    return cmd;
}

static cmdt parse_cmd_type(parser* p) {
    cmdt type = Illegal;
    switch (p->ch) {
    case '$': {
        object obj = parse_object(p);
        if (obj.type != String) {
            return type;
        }
        type = lookup_cmd(&obj.data.string);
        object_free(&obj);
    } break;
    case '+':
        type = parse_simple_string_cmd(p);
        break;
    default:
        break;
    }
    return type;
}

static cmdt lookup_cmd(vstr* s) {
    size_t i;
    size_t s_len = vstr_len(s);
    const char* s_data = vstr_data(s);
    for (i = 0; i < lookup_len; ++i) {
        cmdt_lookup l = lookup[i];
        if (l.cmd_len != s_len) {
            continue;
        }
        if (memcmp(s_data, l.cmd, s_len) == 0) {
            return l.type;
        }
    }
    return Illegal;
}

static object parse_object(parser* p) {
    object obj = {0};
    switch (p->ch) {
    case '$': {
        vstr s;
        uint64_t len;
        if (!expect_peek_byte_to_be_num(p)) {
            return obj;
        }

        len = parse_len(p);

        if (!cur_byte_is(p, '\r')) {
            return obj;
        }
        if (!expect_peek_byte(p, '\n')) {
            return obj;
        }

        parser_read_char(p);

        s = parse_string(p, len);

        if (!cur_byte_is(p, '\r')) {
            return obj;
        }
        if (!expect_peek_byte(p, '\n')) {
            return obj;
        }
        obj = object_new(String, &s);
        parser_read_char(p);
    } break;
    case ':': {
        int64_t val = parse_integer(p);

        if (!cur_byte_is(p, '\r')) {
            return obj;
        }
        if (!expect_peek_byte(p, '\n')) {
            return obj;
        }

        obj = object_new(Int, &val);
        parser_read_char(p);
    } break;
    default:
        break;
    }
    return obj;
}

static vstr parse_string(parser* p, uint64_t len) {
    size_t i;
    vstr s = vstr_new_len(len + 1);
    for (i = 0; i < len; ++i) {
        vstr_push_char(&s, p->ch);
        parser_read_char(p);
    }

    return s;
}

static cmdt parse_simple_string_cmd(parser* p) {
    cmdt type = Illegal;
    vstr s = vstr_new_len(5);
    s = parse_simple_string(p);
    parser_read_char(p);
    type = lookup_cmd(&s);
    vstr_free(&s);
    return type;
}

static vstr parse_simple_string(parser* p) {
    vstr s = vstr_new_len(5);
    parser_read_char(p);
    while (p->ch != '\r') {
        vstr_push_char(&s, p->ch);
        parser_read_char(p);
    }
    if (!cur_byte_is(p, '\r')) {
        vstr_free(&s);
        return s;
    }
    if (!expect_peek_byte(p, '\n')) {
        vstr_free(&s);
        return s;
    }
    return s;
}

static int64_t parse_integer(parser* p) {
    int64_t res;
    uint64_t tmp = 0;
    uint8_t shift = 56;
    uint8_t i;
    parser_read_char(p);
    for (i = 0; i < 8; ++i, shift -= 8) {
        tmp |= ((uint64_t)(p->ch)) << shift;
        parser_read_char(p);
    }
    if (tmp <= 0x7fffffffffffffffu) {
        res = tmp;
    } else {
        res = -1 - (int64_t)(0xffffffffffffffffu - tmp);
    }
    return res;
}

static uint64_t parse_len(parser* p) {
    uint64_t res = 0;
    while (isdigit(p->ch)) {
        res = (res * 10) + (p->ch - '0');
        parser_read_char(p);
    }
    return res;
}

static bool inline cur_byte_is(parser* p, uint8_t byte) {
    return p->ch == byte;
}

static bool peek_byte_is(parser* p, uint8_t byte) {
    return peek_byte(p) == byte;
}

static bool expect_peek_byte(parser* p, uint8_t byte) {
    if (peek_byte_is(p, byte)) {
        parser_read_char(p);
        return true;
    }
    return false;
}

static bool expect_peek_byte_to_be_num(parser* p) {
    uint8_t peek = peek_byte(p);
    if (isdigit(peek)) {
        parser_read_char(p);
        return true;
    }
    return false;
}

static inline void parser_read_char(parser* p) {
    if (p->pos >= p->input_len) {
        p->ch = 0;
        return;
    }
    p->ch = p->input[p->pos];
    p->pos++;
}

static uint8_t peek_byte(parser* p) {
    if (p->pos >= p->input_len) {
        return 0;
    }
    return p->input[p->pos];
}
