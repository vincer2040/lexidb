#include "parser.h"
#include "cmd.h"
#include "object.h"
#include "vstr.h"
#include <ctype.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
    {"OK", 2, Okc},    {"SET", 3, Set},     {"GET", 3, Get},
    {"DEL", 3, Del},   {"POP", 3, Pop},     {"HELP", 4, Help},
    {"AUTH", 4, Auth}, {"PING", 4, Ping},   {"INFO", 4, Infoc},
    {"PUSH", 4, Push}, {"ZSET", 4, ZSet},   {"ZHAS", 4, ZHas},
    {"ZDEL", 4, ZDel}, {"ENQUE", 5, Enque}, {"DEQUE", 5, Deque},
};

const size_t lookup_len = sizeof lookup / sizeof lookup[0];

static parser parser_new(const uint8_t* input, size_t input_len);
static cmd parse_cmd(parser* p);
static cmd parse_array_cmd(parser* p);
static cmdt parse_cmd_type(parser* p);
static cmdt lookup_cmd(vstr* s);
static cmdt parse_simple_string_cmd(parser* p);
static cmdt parse_bulk_string_cmd(parser* p);
static object parse_object(parser* p);
static vstr parse_string(parser* p, uint64_t len);
static vstr parse_simple_string(parser* p);
static vstr parse_error(parser* p);
static int64_t parse_integer(parser* p);
static double parse_double(parser* p);
static uint64_t parse_len(parser* p);
static uint8_t peek_byte(parser* p);
static inline bool cur_byte_is(parser* p, uint8_t byte);
static inline bool peek_byte_is(parser* p, uint8_t byte);
static bool expect_peek_byte(parser* p, uint8_t byte);
static bool expect_peek_byte_to_be_num(parser* p);
static inline void parser_read_char(parser* p);
static int parser_object_cmp(void* a, void* b);
static void parser_free_object(void* ptr);

cmd parse(const uint8_t* input, size_t input_len) {
    parser p = parser_new(input, input_len);
    return parse_cmd(&p);
}

object parse_from_server(const uint8_t* input, size_t input_len) {
    parser p = parser_new(input, input_len);
    return parse_object(&p);
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
    case '$':
        cmd.type = parse_bulk_string_cmd(p);
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
    case Help: {
        cmdt help_cmd = parse_bulk_string_cmd(p);
        if (help_cmd == Illegal) {
            return cmd;
        }
        cmd.type = Help;
        cmd.data.help.wants_cmd_help = 1;
        cmd.data.help.cmd_to_help = help_cmd;
    } break;
    case Auth: {
        object username;
        object password;
        auth_cmd auth = {0};
        if (len != 3) {
            return cmd;
        }
        username = parse_object(p);
        if (username.type == Null) {
            return cmd;
        }
        password = parse_object(p);
        if (password.type == Null) {
            object_free(&username);
            return cmd;
        }
        auth.username = username;
        auth.password = password;
        cmd.data.auth = auth;
        cmd.type = Auth;
    } break;
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
    case Push: {
        object value;
        push_cmd push = {0};
        if (len != 2) {
            return cmd;
        }
        value = parse_object(p);
        if (value.type == Null) {
            return cmd;
        }
        push.value = value;
        cmd.type = Push;
        cmd.data.push = push;
    } break;
    case Enque: {
        object value;
        enque_cmd enque = {0};
        if (len != 2) {
            return cmd;
        }
        value = parse_object(p);
        if (value.type == Null) {
            return cmd;
        }
        enque.value = value;
        cmd.type = Enque;
        cmd.data.enque = enque;
    } break;
    case ZSet: {
        object value;
        zset_cmd zset = {0};
        if (len != 2) {
            return cmd;
        }
        value = parse_object(p);
        if (value.type == Null) {
            return cmd;
        }
        zset.value = value;
        cmd.type = ZSet;
        cmd.data.zset = zset;
    } break;
    case ZHas: {
        object value;
        zhas_cmd zhas = {0};
        if (len != 2) {
            return cmd;
        }
        value = parse_object(p);
        if (value.type == Null) {
            return cmd;
        }
        zhas.value = value;
        cmd.type = ZHas;
        cmd.data.zhas = zhas;
    } break;
    case ZDel: {
        object value;
        zdel_cmd zdel = {0};
        if (len != 2) {
            return cmd;
        }
        value = parse_object(p);
        if (value.type == Null) {
            return cmd;
        }
        zdel.value = value;
        cmd.type = ZDel;
        cmd.data.zdel = zdel;
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
    case ',': {
        double val = parse_double(p);

        if (!cur_byte_is(p, '\r')) {
            return obj;
        }
        if (!expect_peek_byte(p, '\n')) {
            return obj;
        }

        obj = object_new(Double, &val);
        parser_read_char(p);
    } break;
    case '+': {
        vstr s = parse_simple_string(p);
        obj = object_new(String, &s);
        parser_read_char(p);
    } break;
    case '-': {
        vstr s = parse_error(p);
        obj = object_new(String, &s);
        parser_read_char(p);
    } break;
    case '*': {
        uint64_t i, len;
        vec* vec;
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

        vec = vec_new(sizeof(object));

        for (i = 0; i < len; ++i) {
            object cur = parse_object(p);
            vec_push(&vec, &cur);
        }

        obj.type = Array;
        obj.data.vec = vec;
    } break;
    case '%': {
        uint64_t i, len;
        ht ht;
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

        ht = ht_new(sizeof(object), parser_object_cmp);

        for (i = 0; i < len; ++i) {
            object key = parse_object(p);
            object value = parse_object(p);
            ht_insert(&ht, &key, sizeof(object), &value, parser_free_object,
                      parser_free_object);
        }
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

static cmdt parse_bulk_string_cmd(parser* p) { return parse_cmd_type(p); }

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
    while (p->ch != '\r' && p->ch != 0) {
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

static vstr parse_error(parser* p) {
    vstr s = vstr_new();
    parser_read_char(p);
    while (p->ch != '\r' && p->ch != 0) {
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

static double parse_double(parser* p) {
    double res = 0;
    vstr number_string = vstr_new();
    char* end_ptr = NULL;
    const char* num_str;
    parser_read_char(p);
    while (p->ch != '\r') {
        vstr_push_char(&number_string, p->ch);
        parser_read_char(p);
    }
    num_str = vstr_data(&number_string);
    res = strtod(num_str, &end_ptr);
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

static inline bool cur_byte_is(parser* p, uint8_t byte) {
    return p->ch == byte;
}

static inline bool peek_byte_is(parser* p, uint8_t byte) {
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

static int parser_object_cmp(void* a, void* b) { return object_cmp(a, b); }

static void parser_free_object(void* ptr) { object_free(ptr); }
