#include "vjson.h"
#include <ctype.h>
#include <memory.h>
#include <stddef.h>

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

static json_token json_lexer_read_string(json_lexer* l);
static json_token json_lexer_read_number(json_lexer* l);
static json_token json_lexer_read_json_token(json_lexer* l);
static int is_start_of_json_number(json_lexer* l);
static int is_valid_json_number_byte(json_lexer* l);
static void json_lexer_read_char(json_lexer* l);
static int is_json_whitespace(unsigned char byte);
static void json_lexer_skip_whitespace(json_lexer* l);

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
    res.start = l->input + (l->pos);
    json_lexer_read_char(l);
    while (l->byte != '"' && l->byte != 0) {
        json_lexer_read_char(l);
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
