#include "vjson.h"
#include "ht.h"
#include "vstr.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
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
static vjson_object* json_parser_parse(json_parser* p);
static vjson_object* json_parser_parse_data(json_parser* p);
static vjson_object* json_parser_parse_null(json_parser* p);
static vjson_object* json_parser_parse_number(json_parser* p);
static vjson_object* json_parser_parse_boolean(json_parser* p);
static vjson_object* json_parser_parse_string(json_parser* p);
static vjson_object* json_parser_parse_array(json_parser* p);
static vjson_object* json_parser_parse_object(json_parser* p);

static int json_parser_peek_token_is(json_parser* p, json_token_t type);
static int json_parser_expect_peek(json_parser* p, json_token_t type);
static void json_parser_next_token(json_parser* p);

static int is_start_of_json_number(json_lexer* l);
static int is_valid_json_number_byte(json_lexer* l);

static json_lexer json_lexer_new(const unsigned char* input, size_t input_len);
static json_token json_lexer_next_token(json_lexer* l);
static json_token json_lexer_read_string(json_lexer* l);
static json_token json_lexer_read_number(json_lexer* l);
static json_token json_lexer_read_json_token(json_lexer* l);

static void json_lexer_read_char(json_lexer* l);
static int is_json_whitespace(unsigned char byte);
static void json_lexer_skip_whitespace(json_lexer* l);
static void json_object_free_in_structure(void* ptr);
static vstr json_parser_parse_string_value(json_parser* p, int* success);

static vstr object_to_string(const vjson_object* obj);
static vstr json_string_to_string(const vstr* s);
static vstr json_vec_to_string(const vec* obj);
static vstr json_object_to_string(const ht* obj);

vjson_object* vjson_parse(const unsigned char* input, size_t input_len) {
    json_lexer l = json_lexer_new(input, input_len);
    json_parser p = json_parser_new(&l);
    return json_parser_parse(&p);
}

void vjson_object_free(vjson_object* obj) {
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
        vec_free(obj->data.array, json_object_free_in_structure);
        break;
    case JOT_Object:
        ht_free(&obj->data.object, NULL, json_object_free_in_structure);
        break;
    }
    free(obj);
}

vstr vjson_object_to_string(vjson_object* obj) { return object_to_string(obj); }

static vstr object_to_string(const vjson_object* obj) {
    vstr s;
    switch (obj->type) {
    case JOT_Null:
        s = vstr_from("null");
        break;
    case JOT_Number:
        s = vstr_format("%g", obj->data.number);
        break;
    case JOT_Bool:
        s = vstr_from(obj->data.boolean ? "true" : "false");
        break;
    case JOT_String:
        s = json_string_to_string(&obj->data.string);
        break;
    case JOT_Array:
        s = json_vec_to_string(obj->data.array);
        break;
    case JOT_Object:
        s = json_object_to_string(&obj->data.object);
        break;
    }
    return s;
}

static vstr json_string_to_string(const vstr* s) {
    vstr res = vstr_new();
    size_t i, len = vstr_len((vstr*)s);
    const char* cstr = vstr_data((vstr*)s);
    vstr_push_char(&res, '"');
    for (i = 0; i < len; ++i) {
        char ch = cstr[i];
        switch (ch) {
        case '\\':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, '\\');
            break;
        case '"':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, '"');
            break;
        case '/':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, '/');
            break;
        case '\b':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, 'b');
            break;
        case '\f':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, 'f');
            break;
        case '\n':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, 'n');
            break;
        case '\r':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, 'r');
            break;
        case '\t':
            vstr_push_char(&res, '\\');
            vstr_push_char(&res, 't');
            break;
        // TODO: \u then 4 hex digits
        default:
            vstr_push_char(&res, ch);
            break;
        }
    }
    vstr_push_char(&res, '"');
    return res;
}

static vstr json_vec_to_string(const vec* obj) {
    vstr res = vstr_new();
    size_t i = 0, len = obj->len;
    vec_iter iter = vec_iter_new((vec*)obj);
    vstr_push_char(&res, '[');
    while (iter.cur) {
        vjson_object** cur = iter.cur;
        vstr s = object_to_string(*cur);
        vstr_push_string_len(&res, vstr_data(&s), vstr_len(&s));
        vstr_free(&s);
        if (i != len - 1) {
            vstr_push_char(&res, ',');
        }
        vec_iter_next(&iter);
        i++;
    }
    vstr_push_char(&res, ']');
    return res;
}

static vstr json_object_to_string(const ht* obj) {
    vstr res = vstr_new();
    ht_iter iter = ht_iter_new((ht*)obj);
    size_t i = 0, len = obj->num_entries;
    vstr_push_char(&res, '{');
    while (iter.cur) {
        ht_entry* cur = iter.cur;
        const char* key = ht_entry_get_key(cur);
        const vjson_object** value =
            (const vjson_object**)ht_entry_get_value(cur);
        vstr value_str = object_to_string(*value);

        vstr_push_char(&res, '"');
        vstr_push_string_len(&res, key, cur->key_size);
        vstr_push_char(&res, '"');
        vstr_push_char(&res, ':');
        vstr_push_string_len(&res, vstr_data(&value_str), vstr_len(&value_str));
        vstr_free(&value_str);

        if (i != len - 1) {
            vstr_push_char(&res, ',');
        }
        ht_iter_next(&iter);
        i++;
    }
    vstr_push_char(&res, '}');
    return res;
}

static json_parser json_parser_new(json_lexer* l) {
    json_parser p = {0};
    p.l = *l;
    json_parser_next_token(&p);
    json_parser_next_token(&p);
    return p;
}

static vjson_object* json_parser_parse(json_parser* p) {
    return json_parser_parse_data(p);
}

static vjson_object* json_parser_parse_data(json_parser* p) {
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
    case JT_LSquirly:
        return json_parser_parse_object(p);
    default:
        break;
    }
    return NULL;
}

static vjson_object* json_parser_parse_null(json_parser* p) {
    vjson_object* obj = calloc(1, sizeof *obj);
    UNUSED(p);
    if (obj == NULL) {
        return NULL;
    }
    obj->type = JOT_Null;
    return obj;
}

static vjson_object* json_parser_parse_number(json_parser* p) {
    size_t len = p->cur.end - p->cur.start + 1;
    vjson_object* obj = calloc(1, sizeof *obj);
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

static vjson_object* json_parser_parse_boolean(json_parser* p) {
    int res;
    vjson_object* obj;
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

static vjson_object* json_parser_parse_string(json_parser* p) {
    size_t i, len = p->cur.end - p->cur.start;
    vjson_object* obj = calloc(1, sizeof *obj);
    vstr s;
    int success = 0;
    if (obj == NULL) {
        return NULL;
    }
    s = json_parser_parse_string_value(p, &success);
    if (success != 0) {
        free(obj);
        return NULL;
    }
    obj->type = JOT_String;
    obj->data.string = s;
    return obj;
}

static vstr json_parser_parse_string_value(json_parser* p, int* success) {
    vstr s = vstr_new();
    size_t i, len = p->cur.end - p->cur.start;
    for (i = 0; i < len; ++i) {
        char ch = p->cur.start[i];
        if (ch == '\\') {
            char next_ch;
            i += 1;
            if (i > len) {
                // TODO: emit invaid '\'
                vstr_free(&s);
                *success = -1;
                return s;
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
            case 'f':
                vstr_push_char(&s, '\f');
                break;
            case 'b':
                vstr_push_char(&s, '\b');
                break;
            // TODO: 4 hex digits after \u
            default:
                // TODO: emmit bad control
                vstr_free(&s);
                *success = -1;
                return s;
            }
            continue;
        }
        vstr_push_char(&s, p->cur.start[i]);
    }
    return s;
}

static vjson_object* json_parser_parse_array(json_parser* p) {
    vjson_object* obj;
    vec* v;
    vjson_object* item;
    obj = calloc(1, sizeof *obj);
    if (obj == NULL) {
        return NULL;
    }
    v = vec_new(sizeof(vjson_object*));
    if (v == NULL) {
        free(obj);
        return NULL;
    }
    if (json_parser_peek_token_is(p, JT_RBracket)) {
        obj->type = JOT_Array;
        obj->data.array = v;
        json_parser_next_token(p);
        return obj;
    }
    json_parser_next_token(p);
    item = json_parser_parse_data(p);
    if (item == NULL) {
        vec_free(v, json_object_free_in_structure);
        free(obj);
        return NULL;
    }
    vec_push(&v, &item);

    while (json_parser_peek_token_is(p, JT_Comma)) {
        json_parser_next_token(p);
        json_parser_next_token(p);
        item = json_parser_parse_data(p);
        if (item == NULL) {
            vec_free(v, json_object_free_in_structure);
            free(obj);
            return NULL;
        }
        vec_push(&v, &item);
    }
    if (!json_parser_expect_peek(p, JT_RBracket)) {
        vec_free(v, json_object_free_in_structure);
        free(obj);
        return NULL;
    }
    obj->type = JOT_Array;
    obj->data.array = v;
    return obj;
}

static vjson_object* json_parser_parse_object(json_parser* p) {
    vjson_object* obj = calloc(1, sizeof *obj);
    ht ht = ht_new(sizeof(vjson_object*), NULL);
    vjson_object* item;
    vstr key;
    ht_result insert_res;
    int success = 0;
    if (obj == NULL) {
        return NULL;
    }
    if (json_parser_peek_token_is(p, JT_RSquirly)) {
        obj->type = JOT_Object;
        obj->data.object = ht;
        json_parser_next_token(p);
        return obj;
    }

    if (!json_parser_expect_peek(p, JT_String)) {
        ht_free(&ht, NULL, json_object_free_in_structure);
        free(obj);
        return NULL;
    }

    key = json_parser_parse_string_value(p, &success);

    if (!json_parser_expect_peek(p, JT_Colon)) {
        ht_free(&ht, NULL, json_object_free_in_structure);
        vstr_free(&key);
        free(obj);
        return NULL;
    }

    json_parser_next_token(p);

    item = json_parser_parse_data(p);
    if (item == NULL) {
        ht_free(&ht, NULL, json_object_free_in_structure);
        vstr_free(&key);
        free(obj);
        return NULL;
    }

    insert_res = ht_insert(&ht, (void*)vstr_data(&key), vstr_len(&key), &item,
                           NULL, json_object_free_in_structure);
    if (insert_res != HT_OK) {
        ht_free(&ht, NULL, json_object_free_in_structure);
        vstr_free(&key);
        free(obj);
        return NULL;
    }

    vstr_free(&key);

    while (json_parser_peek_token_is(p, JT_Comma)) {
        json_parser_next_token(p);

        if (!json_parser_expect_peek(p, JT_String)) {
            ht_free(&ht, NULL, json_object_free_in_structure);
            free(obj);
            return NULL;
        }

        key = json_parser_parse_string_value(p, &success);

        if (!json_parser_expect_peek(p, JT_Colon)) {
            ht_free(&ht, NULL, json_object_free_in_structure);
            vstr_free(&key);
            free(obj);
            return NULL;
        }

        json_parser_next_token(p);

        item = json_parser_parse_data(p);
        if (item == NULL) {
            ht_free(&ht, NULL, json_object_free_in_structure);
            vstr_free(&key);
            free(obj);
            return NULL;
        }

        insert_res = ht_insert(&ht, (void*)vstr_data(&key), vstr_len(&key),
                               &item, NULL, json_object_free_in_structure);
        if (insert_res != HT_OK) {
            ht_free(&ht, NULL, json_object_free_in_structure);
            vstr_free(&key);
            free(obj);
            return NULL;
        }

        vstr_free(&key);
    }

    if (!json_parser_expect_peek(p, JT_RSquirly)) {
        ht_free(&ht, NULL, json_object_free_in_structure);
        free(obj);
        return NULL;
    }

    obj->type = JOT_Object;
    obj->data.object = ht;
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
        if (prev == '\\' && l->byte == '\\') {
            prev = 0;
        } else {
            prev = l->byte;
        }
        json_lexer_read_char(l);
        if (l->byte == '"' && prev != '\\') {
            break;
        }
    }
    if (l->byte != '"') {
        res.type = JT_Illegal;
        res.start = res.end = NULL;
        return res;
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

static void json_object_free_in_structure(void* ptr) {
    vjson_object* obj = *((vjson_object**)ptr);
    vjson_object_free(obj);
}
