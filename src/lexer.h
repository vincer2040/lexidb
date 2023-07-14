#ifndef __LEXER_H__

#define __LEXER_H__

#include <stdint.h>
#include <stddef.h>
#include "token.h"

typedef struct {
    uint8_t* input;
    size_t pos;
    size_t read_pos;
    uint8_t ch;
    size_t inp_len;
} Lexer;

Lexer lexer_new(uint8_t* buf, size_t buf_len);
Token lexer_next_token(Lexer* l);
void print_tok(Token t);

#endif
