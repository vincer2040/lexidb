#ifndef __HILEXI_PARSER_H__

#define __HILEXI_PARSER_H__

#include "hilexi.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HLT_SIMPLE,
    HLT_NEWL,
    HLT_RETCAR,
    HLT_BULK_TYPE,
    HLT_ARR_TYPE,
    HLT_BULK,
    HLT_INT,
    HLT_LEN,
    HLT_EOF,
} HLTokenT;

typedef struct {
    HLTokenT type;
    uint8_t* literal;
} HLToken;

typedef struct {
    uint8_t* input;
    size_t input_len;
    size_t pos;
    uint8_t ch;
} HLLexer;

typedef struct {
    HLLexer l;
    HLToken cur;
    HLToken next;
} HLParser;

HLLexer hl_lexer_new(uint8_t* input, size_t input_len);
HLParser hl_parser_new(HLLexer* l);
HiLexiData hl_parse(HLParser* p);

#endif /* __HILEXI_PARSER_H__ */
