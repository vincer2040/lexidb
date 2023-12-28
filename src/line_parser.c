#include "cmd.h"
#include "vstr.h"
#include <ctype.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    const char* line;
    size_t line_len;
    size_t pos;
    char ch;
} line_parser;

typedef struct {
    const char* cmd_str;
    size_t cmd_str_len;
    cmdt type;
} lookup;

const lookup lookups[] = {
    {"set", 3, Set},     {"SET", 3, Set},     {"get", 3, Get},
    {"GET", 3, Get},     {"del", 3, Del},     {"DEL", 3, Del},
    {"pop", 3, Pop},     {"POP", 3, Pop},     {"ping", 4, Ping},
    {"PING", 4, Ping},   {"push", 4, Push},   {"PUSH", 4, Push},
    {"zset", 4, ZSet},   {"ZSET", 4, ZSet},   {"zhas", 4, ZHas},
    {"ZHAS", 4, ZHas},   {"zdel", 4, ZDel},   {"ZDEL", 4, ZDel},
    {"enque", 5, Enque}, {"ENQUE", 5, Enque}, {"deque", 5, Deque},
    {"DEQUE", 5, Deque},
};

size_t lookups_len = sizeof lookups / sizeof lookups[0];

static line_parser line_parser_new(const char* line, size_t line_len);
static cmd parse_cmd(line_parser* p);
static cmdt parse_cmd_type(line_parser* p);
static cmdt lookup_cmd(vstr* s);
static object parse_object(line_parser* p);
static vstr parse_string(line_parser* p);
static int64_t parse_number(line_parser* p);
static vstr parse_bulk_string(line_parser* p);
static vstr parse_bulk_string(line_parser* p);
static void line_parser_read_char(line_parser* p);
static void skip_whitespace(line_parser* p);
static inline bool is_letter(char ch);
static inline bool is_start_of_bulk(char ch);
static inline bool is_negative_int(char ch);
static bool is_start_of_number(line_parser* p);
static char peek_char(line_parser* p);

cmd parse_line(const char* line, size_t line_len) {
    line_parser p = line_parser_new(line, line_len);
    return parse_cmd(&p);
}

static line_parser line_parser_new(const char* line, size_t line_len) {
    line_parser p = {0};
    p.line = line;
    p.line_len = line_len;
    line_parser_read_char(&p);
    return p;
}

static cmd parse_cmd(line_parser* p) {
    cmd cmd = {0};
    cmdt type = parse_cmd_type(p);
    skip_whitespace(p);
    switch (type) {
    case Ping:
        cmd.type = Ping;
        break;
    case Pop:
        cmd.type = Pop;
        break;
    case Set: {
        object key = parse_object(p);
        object value = parse_object(p);
        cmd.data.set.key = key;
        cmd.data.set.value = value;
        cmd.type = Set;
    } break;
    case Get: {
        object key = parse_object(p);
        cmd.data.get.key = key;
        cmd.type = Get;
    } break;
    case Del: {
        object key = parse_object(p);
        cmd.data.del.key = key;
        cmd.type = Del;
    } break;
    case Push: {
        object value = parse_object(p);
        cmd.data.push.value = value;
        cmd.type = Push;
    } break;
    case Enque: {
        object value = parse_object(p);
        cmd.data.enque.value = value;
        cmd.type = Enque;
    } break;
    case Deque:
        cmd.type = Deque;
        break;
    case ZSet: {
        object value = parse_object(p);
        cmd.data.zset.value = value;
        cmd.type = ZSet;
    } break;
    case ZHas: {
        object value = parse_object(p);
        cmd.data.zhas.value = value;
        cmd.type = ZHas;
    } break;
    case ZDel: {
        object value = parse_object(p);
        cmd.data.zdel.value = value;
        cmd.type = ZDel;
    } break;
    default:
        cmd.type = Illegal;
        break;
    }
    return cmd;
}

static cmdt parse_cmd_type(line_parser* p) {
    cmdt type = Illegal;
    vstr s = vstr_new();
    skip_whitespace(p);
    while (p->ch != ' ' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        line_parser_read_char(p);
    }
    type = lookup_cmd(&s);
    vstr_free(&s);
    return type;
}

static cmdt lookup_cmd(vstr* s) {
    size_t i;
    size_t s_len = vstr_len(s);
    const char* str = vstr_data(s);
    for (i = 0; i < lookups_len; ++i) {
        lookup l = lookups[i];
        if (l.cmd_str_len != s_len) {
            continue;
        }
        if (memcmp(l.cmd_str, str, l.cmd_str_len) == 0) {
            return l.type;
        }
    }
    return Illegal;
}

static object parse_object(line_parser* p) {
    object obj;
    skip_whitespace(p);
    if (is_letter(p->ch)) {
        vstr s = parse_string(p);
        obj = object_new(String, &s);
    } else if (is_start_of_bulk(p->ch)) {
        vstr s = parse_bulk_string(p);
        obj = object_new(String, &s);
    } else if (is_start_of_number(p)) {
        int64_t num = parse_number(p);
        obj = object_new(Int, &num);
    } else {
        obj = object_new(Null, NULL);
    }
    return obj;
}

static vstr parse_string(line_parser* p) {
    vstr s = vstr_new();
    while (p->ch != ' ' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        line_parser_read_char(p);
    }
    return s;
}

static vstr parse_bulk_string(line_parser* p) {
    vstr s = vstr_new();
    line_parser_read_char(p);
    while (p->ch != '"' && p->ch != 0) {
        if (p->ch == '\\') {
            char peek = peek_char(p);
            switch (peek) {
            case 'n':
                vstr_push_char(&s, '\n');
                line_parser_read_char(p);
                break;
            case 'r':
                vstr_push_char(&s, '\r');
                line_parser_read_char(p);
                break;
            case 't':
                vstr_push_char(&s, '\t');
                line_parser_read_char(p);
                break;
            case '"':
                vstr_push_char(&s, '"');
                line_parser_read_char(p);
                break;
            default:
                vstr_push_char(&s, '\\');
                break;
            }
            line_parser_read_char(p);
            continue;
        }

        vstr_push_char(&s, p->ch);

        line_parser_read_char(p);
    }
    line_parser_read_char(p);
    return s;
}

static void line_parser_read_char(line_parser* p) {
    if (p->pos >= p->line_len) {
        p->ch = 0;
        return;
    }
    p->ch = p->line[p->pos];
    p->pos++;
}

static int64_t parse_number(line_parser* p) {
    int64_t num = 0;
    bool is_negative = false;
    if (is_negative_int(p->ch)) {
        is_negative = true;
        line_parser_read_char(p);
    }
    while (isdigit(p->ch)) {
        num = (num * 10) + (p->ch - '0');
        line_parser_read_char(p);
    }
    if (is_negative) {
        num = -num;
    }
    return num;
}

static void skip_whitespace(line_parser* p) {
    while (p->ch == ' ' || p->ch == '\t') {
        line_parser_read_char(p);
    }
}

static inline bool is_letter(char ch) {
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
}

static inline bool is_start_of_bulk(char ch) { return ch == '"'; }

static bool is_start_of_number(line_parser* p) {
    if (isdigit(p->ch)) {
        return true;
    }

    if (p->ch == '-') {
        if (isdigit(peek_char(p))) {
            return true;
        }
    }
    return false;
}

static inline bool is_negative_int(char ch) { return ch == '-'; }

static char peek_char(line_parser* p) {
    if (p->pos >= p->line_len) {
        return 0;
    }
    return p->line[p->pos];
}
