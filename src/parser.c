#include "parser.h"
#include "object.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIMPLE_STRING_BYTE '+'
#define SIMPLE_ERROR_BYTE '-'
#define INTEGER_BYTE ':'
#define BULK_STRING_BYTE '$'
#define BULK_ERROR_BYTE '!'
#define ARRAY_BYTE '*'
#define NULL_BYTE '_'
#define BOOLEAN_BYTE '#'
#define DOUBLE_BYTE ','

#define CR_BYTE '\r'
#define LF_BYTE '\n'

static object parser_parse_object(parser* p);
static object parser_parse_object_v1(parser* p);
static object parser_parse_simple_string(parser* p);
static object parser_parse_bulk_string(parser* p);
static object parser_parse_simple_error(parser* p);
static object parser_parse_bulk_error(parser* p);
static object parser_parse_int(parser* p);
static object parser_parse_double(parser* p);
static object parser_parse_boolean(parser* p);
static object parser_parse_null(parser* p);
static size_t parser_parse_length(parser* p);
static int parser_expect_peek_to_be_length(parser* p);
static int parser_expect_peek(parser* p, uint8_t byte);
static uint8_t parser_peek_byte(parser* p);
static vstr expected_cr(uint8_t got);
static vstr expected_lf(uint8_t got);
static void parser_read_byte(parser* p);

parser parser_new(const uint8_t* input, size_t input_len, int version) {
    parser p = {0};
    p.input = input;
    p.input_len = input_len;
    p.version = version;
    parser_read_byte(&p);
    return p;
}

int parser_has_error(parser* p) { return p->has_error; }

vstr parser_get_error(parser* p) { return p->error; }

object parse_object(parser* p) { return parser_parse_object(p); }

static object parser_parse_object(parser* p) {
    switch (p->version) {
    case 1:
        return parser_parse_object_v1(p);
    default: {
        vstr err = vstr_format("unkown protocol version: %d\n", p->version);
        p->error = err;
        p->has_error = 1;
    } break;
    }
    return null_obj;
}

static object parser_parse_object_v1(parser* p) {
    switch (p->byte) {
    case SIMPLE_STRING_BYTE:
        return parser_parse_simple_string(p);
    case BULK_STRING_BYTE:
        return parser_parse_bulk_string(p);
    case SIMPLE_ERROR_BYTE:
        return parser_parse_simple_error(p);
    case BULK_ERROR_BYTE:
        return parser_parse_bulk_error(p);
    case INTEGER_BYTE:
        return parser_parse_int(p);
    case DOUBLE_BYTE:
        return parser_parse_double(p);
    case BOOLEAN_BYTE:
        return parser_parse_boolean(p);
    case NULL_BYTE:
        return parser_parse_null(p);
    default:
        break;
    }
    p->error = vstr_format("unkown type byte: %c\n", p->byte);
    p->has_error = 1;
    return null_obj;
}

static object parser_parse_simple_string(parser* p) {
    object obj = {0};
    vstr s = vstr_new();
    parser_read_byte(p);
    while ((p->byte != 0) && (p->byte != CR_BYTE)) {
        vstr_push_char(&s, p->byte);
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        vstr err = expected_cr(p->byte);
        p->error = err;
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        vstr err = expected_lf(parser_peek_byte(p));
        p->error = err;
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    obj.type = OT_String;
    obj.data.string = s;
    parser_read_byte(p);
    return obj;
}

static object parser_parse_bulk_string(parser* p) {
    size_t i, len;
    vstr s;
    object obj = {0};
    if (!parser_expect_peek_to_be_length(p)) {
        p->error = vstr_format("expected peek to be length, got %c\n",
                               parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    len = parser_parse_length(p);
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    parser_read_byte(p);
    s = vstr_new_len(len);
    for (i = 0; i < len; ++i) {
        int push_res = vstr_push_char(&s, p->byte);
        if (push_res == -1) {
            vstr_free(&s);
            p->error = vstr_from("oom\n");
            p->has_error = 1;
            return null_obj;
        }
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    obj.type = OT_String;
    obj.data.string = s;
    return obj;
}

static object parser_parse_bulk_error(parser* p) {
    size_t i, len;
    vstr s;
    object obj = {0};
    if (!parser_expect_peek_to_be_length(p)) {
        p->error = vstr_format("expected peek to be length, got %c\n",
                               parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    len = parser_parse_length(p);
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    parser_read_byte(p);
    s = vstr_new_len(len);
    for (i = 0; i < len; ++i) {
        int push_res = vstr_push_char(&s, p->byte);
        if (push_res == -1) {
            vstr_free(&s);
            p->error = vstr_from("oom\n");
            p->has_error = 1;
            return null_obj;
        }
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    obj.type = OT_Error;
    obj.data.string = s;
    return obj;
}

static object parser_parse_simple_error(parser* p) {
    object obj = {0};
    vstr s = vstr_new();
    parser_read_byte(p);
    while ((p->byte != 0) && (p->byte != CR_BYTE)) {
        vstr_push_char(&s, p->byte);
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        vstr err = expected_cr(p->byte);
        p->error = err;
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        vstr err = expected_lf(parser_peek_byte(p));
        p->error = err;
        p->has_error = 1;
        vstr_free(&s);
        return null_obj;
    }
    obj.type = OT_Error;
    obj.data.string = s;
    parser_read_byte(p);
    return obj;
}

static object parser_parse_int(parser* p) {
    object obj = {0};
    int64_t num = 0;
    int is_negative = 0;
    parser_read_byte(p);
    is_negative = p->byte == '-';
    if (p->byte == '-' || p->byte == '+') {
        parser_read_byte(p);
    } else if (!(isdigit(p->byte))) {
        return null_obj;
    }
    while (isdigit(p->byte)) {
        num = (num * 10) + (p->byte - '0');
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    if (is_negative) {
        num = -num;
    }
    obj.type = OT_Int;
    obj.data.integer = num;
    parser_read_byte(p);
    return obj;
}

static object parser_parse_double(parser* p) {
    object obj = {0};
    const unsigned char* dbl_start = p->input + p->pos;
    const unsigned char* dbl_end;
    double val;
    while ((p->byte != CR_BYTE) && (p->byte != 0)) {
        parser_read_byte(p);
    }
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return null_obj;
    }
    dbl_end = p->input + (p->pos - 1);
    val = strtod((const char*)dbl_start, (char**)&dbl_end);
    if ((val == 0) || (val == HUGE_VAL) || (val == DBL_MIN)) {
        if (errno == ERANGE) {
            p->error = vstr_from(strerror(errno));
            p->has_error = 1;
            return null_obj;
        }
    }
    if (*dbl_end != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    obj.type = OT_Double;
    obj.data.dbl = val;
    parser_read_byte(p);
    return obj;
}

static object parser_parse_boolean(parser* p) {
    uint8_t bool_byte;
    parser_read_byte(p);
    bool_byte = p->byte;
    if (!parser_expect_peek(p, CR_BYTE)) {
        p->error = expected_cr(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    parser_read_byte(p);
    switch (bool_byte) {
    case 't':
        return true_obj;
    case 'f':
        return false_obj;
    default:
        break;
    }
    p->error = vstr_format("unkown boolean byte: %c\n", bool_byte);
    p->has_error = 1;
    return null_obj;
}

static object parser_parse_null(parser* p) {
    if (!parser_expect_peek(p, CR_BYTE)) {
        p->error = expected_cr(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return null_obj;
    }
    parser_read_byte(p);
    return null_obj;
}

static size_t parser_parse_length(parser* p) {
    size_t res = 0;
    while (isdigit(p->byte)) {
        res = (res * 10) + (p->byte - '0');
        parser_read_byte(p);
    }
    return res;
}

static int parser_expect_peek_to_be_length(parser* p) {
    uint8_t peek = parser_peek_byte(p);
    if (!isdigit(peek)) {
        return 0;
    }
    parser_read_byte(p);
    return 1;
}

static int parser_expect_peek(parser* p, uint8_t byte) {
    uint8_t peek = parser_peek_byte(p);
    if (peek != byte) {
        return 0;
    }
    parser_read_byte(p);
    return 1;
}

static uint8_t parser_peek_byte(parser* p) {
    if (p->pos >= p->input_len) {
        return 0;
    }
    return p->input[p->pos];
}

static void parser_read_byte(parser* p) {
    if (p->pos >= p->input_len) {
        p->byte = 0;
        return;
    }
    p->byte = p->input[p->pos];
    p->pos++;
}

static vstr expected_cr(uint8_t got) {
    return vstr_format("expected \\r, got %c\n", got);
}

static vstr expected_lf(uint8_t got) {
    return vstr_format("expected \\n, got %c\n", got);
}
