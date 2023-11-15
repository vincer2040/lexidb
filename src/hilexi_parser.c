#include "hilexi_parser.h"
#include "hilexi.h"
#include "vstring.h"
#include <ctype.h>
#include <memory.h>
#include <stdbool.h>

static void hl_lexer_read_char(HLLexer* l);
static void hl_lexer_read(HLLexer* l);
static HLToken hl_lexer_next_token(HLLexer* l);

static void hl_parser_next_token(HLParser* p);
static bool hl_parser_expect_peek(HLParser* p, HLTokenT type);
static uint64_t hl_parser_parse_len(HLParser* p);

static HiLexiData hl_parse_simple(HLParser* p);
static HiLexiData hl_parser_parse_bulk(HLParser* p);
static HiLexiData hl_parser_parse_arr(HLParser* p);
static HiLexiData hl_parser_parse_int(HLParser* p);
HiLexiData hl_parser_parse_error(HLParser* p);

HLLexer hl_lexer_new(uint8_t* input, size_t input_len) {
    HLLexer l = {0};
    l.input = input;
    l.input_len = input_len;
    hl_lexer_read_char(&l);
    return l;
}

static HLToken hl_lexer_next_token(HLLexer* l) {
    HLToken tok = {0};
    switch (l->ch) {
    case '+':
        tok.type = HLT_SIMPLE;
        tok.literal = l->input + l->pos;
        hl_lexer_read(l);
        return tok;
    case '$':
        tok.type = HLT_BULK_TYPE;
        break;
    case '*':
        tok.type = HLT_ARR_TYPE;
        break;
    case ':':
        tok.type = HLT_INT;
        tok.literal = l->input + l->pos;
        hl_lexer_read(l);
        return tok;
    case '-':
        tok.type = HLT_ERR;
        tok.literal = l->input + l->pos;
        hl_lexer_read(l);
        return tok;
    case '\r':
        tok.type = HLT_RETCAR;
        break;
    case '\n':
        tok.type = HLT_NEWL;
        break;
    default:
        if (isdigit(l->ch)) {
            tok.type = HLT_LEN;
            tok.literal = l->input + l->pos;
            hl_lexer_read(l);
            return tok;
        }
        tok.type = HLT_BULK;
        tok.literal = l->input + l->pos;
        hl_lexer_read(l);
        return tok;
    }
    hl_lexer_read_char(l);
    return tok;
}

static void hl_lexer_read(HLLexer* l) {
    while (l->ch != '\r') {
        hl_lexer_read_char(l);
    }
}

static void hl_lexer_read_char(HLLexer* l) {
    if (l->pos >= l->input_len) {
        l->ch = 0;
    } else {
        l->ch = l->input[l->pos];
    }
    l->pos++;
}

HLParser hl_parser_new(HLLexer* l) {
    HLParser p = {0};
    p.l = *l;
    hl_parser_next_token(&p);
    hl_parser_next_token(&p);
    return p;
}

HiLexiData hl_parse(HLParser* p) {
    HiLexiData data = {0};
    switch (p->cur.type) {
    case HLT_SIMPLE:
        data = hl_parse_simple(p);
        break;
    case HLT_BULK_TYPE:
        data = hl_parser_parse_bulk(p);
        break;
    case HLT_ARR_TYPE:
        data = hl_parser_parse_arr(p);
        break;
    case HLT_INT:
        data = hl_parser_parse_int(p);
        break;
    case HLT_ERR:
        data = hl_parser_parse_error(p);
        break;
    default:
        data.type = HL_ERR;
        data.val.hl_err = HL_INV_DATA;
        break;
    }
    return data;
}

static HiLexiData hl_parse_simple(HLParser* p) {
    HiLexiData data = {0};
    if (memcmp(p->cur.literal, "+PONG", 5) == 0) {
        data.val.simple = HL_PONG;
    } else if (memcmp(p->cur.literal, "+NONE", 5) == 0) {
        data.val.simple = HL_NONE;
    } else if (memcmp(p->cur.literal, "+OK", 3) == 0) {
        data.val.simple = HL_OK;
    } else {
        data.type = HL_ERR;
        data.val.hl_err = HL_INV_DATA;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    hl_parser_next_token(p);
    data.type = HL_SIMPLE_STRING;
    return data;
}

static HiLexiData hl_parser_parse_bulk(HLParser* p) {
    HiLexiData data = {0};
    vstr val;
    size_t i, len;
    uint8_t* buf;

    if (!hl_parser_expect_peek(p, HLT_LEN)) {
        data.type = HL_ERR;
        data.val.hl_err = HL_INV_DATA;
        return data;
    }

    len = hl_parser_parse_len(p);

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_BULK)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    buf = p->cur.literal;

    val = vstr_new_len(len + 1);

    for (i = 0; i < len; ++i) {
        val = vstr_push_char(val, buf[i]);
    }

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        vstr_delete(val);
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        vstr_delete(val);
        return data;
    }

    hl_parser_next_token(p);

    data.type = HL_BULK_STRING;
    data.val.string = val;

    return data;
}

static HiLexiData hl_parser_parse_arr(HLParser* p) {
    HiLexiData data = {0};
    size_t i, len;
    Vec* vec;

    if (!hl_parser_expect_peek(p, HLT_LEN)) {
        data.type = HL_ERR;
        data.val.hl_err = HL_INV_DATA;
        return data;
    }

    len = hl_parser_parse_len(p);

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    hl_parser_next_token(p);

    vec = vec_new(len, sizeof(HiLexiData));

    for (i = 0; i < len; ++i) {
        HiLexiData cur_data = hl_parse(p);
        vec_push(&vec, &cur_data);
    }

    data.type = HL_ARR;
    data.val.arr = vec;

    return data;
}

static HiLexiData hl_parser_parse_int(HLParser* p) {
    HiLexiData data = {0};
    size_t i, len = 9;
    uint8_t* buf = p->cur.literal;
    uint64_t temp = 0;
    uint8_t shift = 56;
    int64_t res;
    for (i = 1; i < len; ++i, shift -= 8) {
        uint8_t at = buf[i];
        temp |= (uint64_t)(at) << shift;
    }

    // hack for checking if value should be negative
    if (temp <= 0x7fffffffffffffffu) {
        res = temp;
    } else {
        res = -1 - (long long int)(0xffffffffffffffffu - temp);
    }

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    hl_parser_next_token(p);

    data.type = HL_INT;
    data.val.integer = res;
    return data;
}

HiLexiData hl_parser_parse_error(HLParser* p) {
    HiLexiData data = { 0 };
    uint8_t* buf = p->cur.literal + 1;
    vstr val = vstr_new();
    while (*buf != '\r') {
        val = vstr_push_char(val, *buf);
        buf++;
    }

    if (!hl_parser_expect_peek(p, HLT_RETCAR)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    if (!hl_parser_expect_peek(p, HLT_NEWL)) {
        data.val.hl_err = HL_INV_DATA;
        data.type = HL_ERR;
        return data;
    }

    hl_parser_next_token(p);

    data.type = HL_ERR_STR;
    data.val.string = val;
    return data;
}

static uint64_t hl_parser_parse_len(HLParser* p) {
    uint64_t res = 0;
    uint8_t* buf = p->cur.literal;
    while (*buf != '\r') {
        res = (res * 10) + (*buf - '0');
        buf++;
    }
    return res;
}

static void hl_parser_next_token(HLParser* p) {
    p->cur = p->next;
    p->next = hl_lexer_next_token(&(p->l));
}

static bool hl_parser_expect_peek(HLParser* p, HLTokenT type) {
    if (p->next.type == type) {
        hl_parser_next_token(p);
        return true;
    }
    return false;
}
