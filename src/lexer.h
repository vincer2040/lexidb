#ifndef __LEXER_H__

#define __LEXER_H__

#include "token.h"
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t* input;
    size_t input_len;
    size_t pos;
    uint8_t ch;
} lexer;

lexer lexer_new(const uint8_t* input, size_t input_len);
token lexer_next_token(lexer* l, size_t bulk_len);

#endif /* __LEXER_H__ */
