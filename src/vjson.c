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

#define UNUSED(v) ((void)v);

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

const char* json_token_type_strings[] = {
    "JT_Illegal",  "JT_Eof",   "JT_LSquirly", "JT_RSquirly", "JT_LBracket",
    "JT_RBracket", "JT_Colon", "JT_Comma",    "JT_Number",   "JT_String",
    "JT_True",     "JT_False", "JT_Null",
};

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
static json_object* json_parser_parse_null(json_parser* p);
static json_object* json_parser_parse_number(json_parser* p);
static json_object* json_parser_parse_boolean(json_parser* p);
static json_object* json_parser_parse_string(json_parser* p);
static json_object* json_parser_parse_array(json_parser* p);
static int json_parser_peek_token_is(json_parser* p, json_token_t type);
static int json_parser_expect_peek(json_parser* p, json_token_t type);
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
static void json_object_free_in_vec(void* ptr);

json_object* vjson_parse(const unsigned char* input, size_t input_len) {
    json_lexer l = json_lexer_new(input, input_len);
    json_parser p = json_parser_new(&l);
    return json_parser_parse(&p);
}

void vjson_object_free(json_object* obj) {
    assert(obj != NULL);
    switch (obj->type) {
    case JOT_Null:
        break;
    case JOT_Number:
        break;
    case JOT_Bool:
        break;
    case JOT_String:
        vstr_free(&obj->data.string);
        break;
    case JOT_Array:
        vec_free(obj->data.array, json_object_free_in_vec);
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
    case JT_Null:
        return json_parser_parse_null(p);
    case JT_Number:
        return json_parser_parse_number(p);
    case JT_True:
    case JT_False:
        return json_parser_parse_boolean(p);
    case JT_String:
        return json_parser_parse_string(p);
    case JT_LBracket:
        return json_parser_parse_array(p);
    default:
        break;
    }
    return NULL;
}

static json_object* json_parser_parse_null(json_parser* p) {
    json_object* obj = calloc(1, sizeof *obj);
    UNUSED(p);
    if (obj == NULL) {
        return NULL;
    }
    obj->type = JOT_Null;
    return obj;
}

static json_object* json_parser_parse_number(json_parser* p) {
    size_t len = p->cur.end - p->cur.start + 1;
    json_object* obj = calloc(1, sizeof *obj);
    vstr dbl_str;
    double res;
    char* end = NULL;
    if (obj == NULL) {
        return NULL;
    }
    dbl_str = vstr_from_len((const char*)(p->cur.start), len);
    res = strtod(vstr_data(&dbl_str), &end);
    // TODO: check errors
    obj->type = JOT_Number;
    obj->data.number = res;
    return obj;
}

static json_object* json_parser_parse_boolean(json_parser* p) {
    int res;
    json_object* obj;
    switch (p->cur.type) {
    case JT_True:
        res = 1;
        break;
    case JT_False:
        res = 0;
        break;
    default:
        assert(0 && "unreachable");
    }
    obj = calloc(1, sizeof *obj);
    if (obj == NULL) {
        return NULL;
    }
    obj->type = JOT_Bool;
    obj->data.boolean = res;
    return obj;
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

static json_object* json_parser_parse_array(json_parser* p) {
    json_object* obj;
    vec* v;
    json_object* item;
    obj = calloc(1, sizeof *obj);
    if (obj == NULL) {
        return NULL;
    }
    v = vec_new(sizeof(json_object*));
    if (v == NULL) {
        free(obj);
        return NULL;
    }
    if (json_parser_peek_token_is(p, JT_RBracket)) {
        obj->type = JOT_Array;
        obj->data.array = v;
        return obj;
    }
    json_parser_next_token(p);
    item = json_parser_parse_data(p);
    if (item == NULL) {
        vec_free(v, json_object_free_in_vec);
        free(obj);
        return NULL;
    }
    vec_push(&v, &item);

    while (json_parser_peek_token_is(p, JT_Comma)) {
        json_parser_next_token(p);
        json_parser_next_token(p);
        item = json_parser_parse_data(p);
        if (item == NULL) {
            vec_free(v, json_object_free_in_vec);
            free(obj);
            return NULL;
        }
        vec_push(&v, &item);
    }
    if (!json_parser_expect_peek(p, JT_RBracket)) {
        vec_free(v, json_object_free_in_vec);
        free(obj);
        return NULL;
    }
    obj->type = JOT_Array;
    obj->data.array = v;
    return obj;
}

static int json_parser_peek_token_is(json_parser* p, json_token_t type) {
    return p->next.type == type;
}

static int json_parser_expect_peek(json_parser* p, json_token_t type) {
    if (!json_parser_peek_token_is(p, type)) {
        return 0;
    }
    json_parser_next_token(p);
    return 1;
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
           l->byte != ']' && l->byte != ',' && l->byte != 0) {
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

static void json_object_free_in_vec(void* ptr) {
    json_object* obj = *((json_object**)ptr);
    vjson_object_free(obj);
}
