#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void print_tok(Token tok) {
    switch (tok.type) {
    case EOFT:
        printf("EOF\n");
        break;
    case OK:
        printf("OK\n");
    case PING:
        printf("PING\n");
        break;
    case TYPE:
        printf("TYPE\n");
        break;
    case LEN:
        printf("LEN\n");
        break;
    case RETCAR:
        printf("RETCAR\n");
        break;
    case NEWL:
        printf("NEWLINE\n");
        break;
    case BULK:
        printf("BULK\n");
        break;
    case INT:
        printf("INT\n");
    case ILLEGAL:
        printf("ILLEGAL\n");
    }
}

void lexer_read_char(Lexer* l) {
    if (l->read_pos >= l->inp_len) {
        l->ch = 0;
    } else {
        l->ch = l->input[l->read_pos];
    }
    l->pos = l->read_pos;
    l->read_pos += 1;
}

void lexer_read_len(Lexer* l) {
    for (; isdigit(l->ch) != 0; lexer_read_char(l)) {
    }
}

void lexer_read_bulk(Lexer* l) {
    for (; l->ch != '\r'; lexer_read_char(l)) {
    }
}

void lexer_read_ping(Lexer* l) {
    size_t i;
    for (i = 0; i < 7; ++i) {
        lexer_read_char(l);
    }
}

void lexer_read_ok(Lexer* l) {
    size_t i;
    for (i = 0; i < 5; ++i) {
        lexer_read_char(l);
    }
}

void lexer_read_int(Lexer* l) {
    size_t i;
    for (i = 0; i < 8; ++i) {
        lexer_read_char(l);
    }
}

#define u8(v) ((uint8_t*)v);

Token lexer_next_token(Lexer* l) {
    Token tok = {0};

    switch (l->ch) {
    case '*':
        tok.type = TYPE;
        tok.literal = u8("*");
        break;
    case '$':
        tok.type = TYPE;
        tok.literal = u8("$");
        break;
    case ':':
        tok.type = INT;
        tok.literal = l->input + l->pos;
        lexer_read_int(l);
        break;
    case '+':
        /**
         * simple strings should be in there own message
         * and not within an array
         */
        if ((l->inp_len == 7) && (memcmp(l->input, "+PING\r\n", 7) == 0)) {
            tok.type = PING;
            tok.literal = u8("+PING\r\n");
            lexer_read_ping(l);
            break;
        } else if ((l->inp_len = 5) && (memcmp(l->input, "+OK\r\n", 5) == 0)) {
            tok.type = OK;
            tok.literal = u8("+OK\r\n");
            lexer_read_ok(l);
            break;
        } else {
            tok.type = ILLEGAL;
            tok.literal = l->input + l->pos;
            break;
        }
    case '\r':
        tok.type = RETCAR;
        tok.literal = u8("\r");
        break;
    case '\n':
        tok.type = NEWL;
        tok.literal = u8("\n");
        break;
    case '\0':
        tok.type = EOFT;
        tok.literal = u8("\0");
        break;
    default:
        if (isdigit(l->ch)) {
            tok.type = LEN;
            tok.literal = l->input + l->pos;
            lexer_read_len(l);
            return tok;
        } else {
            tok.type = BULK;
            tok.literal = l->input + l->pos;
            lexer_read_bulk(l);
            return tok;
        }
    }

    lexer_read_char(l);
    return tok;
}

Lexer lexer_new(uint8_t* buf, size_t buf_len) {
    Lexer l = {0};
    l.input = buf;
    l.inp_len = buf_len;
    lexer_read_char(&l);
    return l;
}
