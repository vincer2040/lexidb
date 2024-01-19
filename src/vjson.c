#include "vjson.h"
#include "vstr.h"
#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define dbgf(...)                                                              \
    {                                                                          \
        printf("%s:%d ", __FILE__, __LINE__);                                  \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    }

typedef enum {
    JT_Illegal,
    JT_Eof,
    JT_LSquirly,
    JT_RSquirly,
    JT_LBracket,
    JT_RBracket,
    JT_Colon,
    JT_Comma,
    JT_Number,
    JT_String,
    JT_True,
    JT_False,
    JT_Null,
} json_token_t;

typedef struct {
    json_token_t type;
    const unsigned char* start;
    const unsigned char* end;
} json_token;

typedef struct {
    const unsigned char* input;
    size_t input_len;
    size_t pos;
    unsigned char byte;
} json_lexer;

typedef struct {
    json_lexer l;
    json_token cur;
    json_token next;
} json_parser;

static json_parser json_parser_new(json_lexer* l);
static json_object* json_parser_parse(json_parser* p);
static json_object* json_parser_parse_data(json_parser* p);
static json_object* json_parser_parse_string(json_parser* p);
static void json_parser_next_token(json_parser* p);
static json_lexer json_lexer_new(const unsigned char* input, size_t input_len);
static json_token json_lexer_next_token(json_lexer* l);
static json_token json_lexer_read_string(json_lexer* l);
static json_token json_lexer_read_number(json_lexer* l);
static json_token json_lexer_read_json_token(json_lexer* l);
static int is_start_of_json_number(json_lexer* l);
static int is_valid_json_number_byte(json_lexer* l);
static void json_lexer_read_char(json_lexer* l);
static int is_json_whitespace(unsigned char byte);
static void json_lexer_skip_whitespace(json_lexer* l);

json_object* vjson_parse(const unsigned char* input, size_t input_len) {
    json_lexer l = json_lexer_new(input, input_len);
    json_parser p = json_parser_new(&l);
    return json_parser_parse(&p);
}

void vjson_object_free(json_object* obj) {
    assert(obj != NULL);
    switch (obj->type) {
    case JOT_String:
        vstr_free(&obj->data.string);
        break;
    case JOT_Number:
        break;
    case JOT_Object:
        break;
    }
    free(obj);
}

static json_parser json_parser_new(json_lexer* l) {
    json_parser p = {0};
    p.l = *l;
    json_parser_next_token(&p);
    json_parser_next_token(&p);
    return p;
}

static json_object* json_parser_parse(json_parser* p) {
    return json_parser_parse_data(p);
}

static json_object* json_parser_parse_data(json_parser* p) {
    switch (p->cur.type) {
    case JT_String:
        return json_parser_parse_string(p);
    default:
        break;
    }
    return NULL;
}

static json_object* json_parser_parse_string(json_parser* p) {
    vstr s = vstr_new();
    size_t i, len = p->cur.end - p->cur.start;
    json_object* obj = calloc(1, sizeof *obj);
    if (obj == NULL) {
        return NULL;
    }
    for (i = 0; i < len; ++i) {
        char ch = p->cur.start[i];
        if (ch == '\\') {
            char next_ch;
            i += 1;
            if (i > len) {
                // TODO: emit invaid '\'
                vstr_free(&s);
                free(obj);
                return NULL;
            }
            next_ch = p->cur.start[i];
            switch (next_ch) {
            case '"':
                vstr_push_char(&s, '"');
                break;
            case '\\':
                vstr_push_char(&s, '\\');
                break;
            case '/':
                vstr_push_char(&s, '/');
                break;
            case 'n':
                vstr_push_char(&s, '\n');
                break;
            case 'r':
                vstr_push_char(&s, '\r');
                break;
            case 't':
                vstr_push_char(&s, '\t');
                break;
            // TODO: 4 hex digits after \u
            default:
                // TODO: emmit bad control
                free(obj);
                vstr_free(&s);
                return NULL;
            }
            continue;
        }
        vstr_push_char(&s, p->cur.start[i]);
    }
    obj->type = JOT_String;
    obj->data.string = s;
    return obj;
}

static void json_parser_next_token(json_parser* p) {
    p->cur = p->next;
    p->next = json_lexer_next_token(&p->l);
}

static json_lexer json_lexer_new(const unsigned char* input, size_t input_len) {
    json_lexer l = {input, input_len, 0, 0};
    json_lexer_read_char(&l);
    return l;
}

static json_token json_lexer_next_token(json_lexer* l) {
    json_token tok = {0};
    json_lexer_skip_whitespace(l);
    switch (l->byte) {
    case '{':
        tok.type = JT_LSquirly;
        break;
    case '}':
        tok.type = JT_RSquirly;
        break;
    case '[':
        tok.type = JT_LBracket;
        break;
    case ']':
        tok.type = JT_RBracket;
        break;
    case ':':
        tok.type = JT_Colon;
        break;
    case ',':
        tok.type = JT_Comma;
        break;
    case '"':
        tok = json_lexer_read_string(l);
        break;
    case 0:
        tok.type = JT_Eof;
        break;
    default:
        if (is_valid_json_number_byte(l)) {
            tok = json_lexer_read_number(l);
            return tok;
        }
        tok = json_lexer_read_json_token(l);
        return tok;
    }
    json_lexer_read_char(l);
    return tok;
}

static json_token json_lexer_read_string(json_lexer* l) {
    json_token res = {0};
    char prev = 0;
    res.start = l->input + (l->pos);
    json_lexer_read_char(l);
    while (l->byte != 0) {
        prev = l->byte;
        json_lexer_read_char(l);
        if (l->byte == '"' && prev != '\\') {
            break;
        }
    }
    res.end = l->input + (l->pos - 1);
    res.type = JT_String;
    return res;
}

static json_token json_lexer_read_number(json_lexer* l) {
    json_token res = {0};
    res.start = l->input + (l->pos - 1);
    while (is_valid_json_number_byte(l)) {
        json_lexer_read_char(l);
    }
    res.end = l->input + (l->pos - 1);
    res.type = JT_Number;
    return res;
}

static json_token json_lexer_read_json_token(json_lexer* l) {
    json_token res = {0};
    char buf[6] = {0};
    size_t i = 0;
    while (i < 6 && !is_json_whitespace(l->byte) && l->byte != '}' &&
           l->byte != ']' && l->byte != ',') {
        buf[i] = l->byte;
        i++;
        json_lexer_read_char(l);
    }
    if (i == 4) {
        if (memcmp(buf, "null", 4) == 0) {
            res.type = JT_Null;
            return res;
        }
        if (memcmp(buf, "true", 4) == 0) {
            res.type = JT_True;
            return res;
        }
        res.type = JT_Illegal;
        return res;
    }
    if (i == 5) {
        if (memcmp(buf, "false", 5) == 0) {
            res.type = JT_False;
            return res;
        }
    }
    res.type = JT_Illegal;
    return res;
}

static void json_lexer_read_char(json_lexer* l) {
    if (l->pos >= l->input_len) {
        l->byte = 0;
        return;
    }
    l->byte = l->input[l->pos];
    l->pos++;
}

static int is_start_of_json_number(json_lexer* l) {
    return isdigit(l->byte) || l->byte == '-';
}

static int is_valid_json_number_byte(json_lexer* l) {
    return isdigit(l->byte) || l->byte == '-' || l->byte == '+' ||
           l->byte == '.' || l->byte == 'e' || l->byte == 'E';
}

static int is_json_whitespace(unsigned char byte) {
    return byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t';
}

static void json_lexer_skip_whitespace(json_lexer* l) {
    while (is_json_whitespace(l->byte)) {
        json_lexer_read_char(l);
    }
}
