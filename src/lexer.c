#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdio.h>

static void lexer_read_char(lexer* l);
static void lexer_read_int(lexer* l);
static void lexer_read_bulk(lexer* l, size_t len);
static void lexer_read_simple(lexer* l, tokent type);

lexer lexer_new(const uint8_t* input, size_t input_len) {
    lexer l = {0};
    l.input = input;
    l.input_len = input_len;
    lexer_read_char(&l);
    return l;
}

token lexer_next_token(lexer* l, size_t bulk_len) {
    token tok = {0};
    switch (l->ch) {
    case '*':
        tok.type = ArrayType;
        break;
    case '$':
        tok.type = BulkType;
        break;
    case '+':
        tok = lookup_simple(l->input + l->pos);
        lexer_read_simple(l, tok.type);
        return tok;
    case '\r':
        tok.type = RetCar;
        break;
    case '\n':
        tok.type = Newl;
        break;
    case 0:
        tok.type = Eof;
        break;
    default:
        if (bulk_len) {
            tok.type = Bulk;
            tok.literal = l->input + (l->pos - 1);
            lexer_read_bulk(l, bulk_len);
            return tok;
        } else if (isdigit(l->ch)) {
            tok.type = Len;
            tok.literal = l->input + (l->pos - 1);
            lexer_read_int(l);
            return tok;
        } else {
            tok.type = Illegal;
        }
        break;
    }
    lexer_read_char(l);
    return tok;
}

static void lexer_read_char(lexer* l) {
    if (l->pos >= l->input_len) {
        l->ch = 0;
        return;
    }
    l->ch = l->input[l->pos];
    l->pos++;
}

static void lexer_read_int(lexer* l) {
    while (isdigit(l->ch)) {
        lexer_read_char(l);
    }
}

static void lexer_read_bulk(lexer* l, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        lexer_read_char(l);
    }
}

static void lexer_read_simple(lexer* l, tokent type) {
    size_t i;
    switch (type) {
    case Ok:
        for (i = 0; i < 3; ++i) {
            lexer_read_char(l);
        }
        break;
    case Ping:
        for (i = 0; i < 5; ++i) {
            lexer_read_char(l);
        }
    default:
        break;
    }
}
