#include "parser.h"
#include "cmd.h"
#include "object.h"
#include "util.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
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

typedef struct {
    size_t len;
    const char* lookup;
    cmd_type type;
} cmd_type_lookup;

const cmd_type_lookup array_cmd_lookups[] = {
    {3, "SET", CT_Set},
    {3, "GET", CT_Get},
    {3, "DEL", CT_Del},
    {4, "AUTH", CT_Auth},
};

const cmd_type_lookup string_cmd_lookups[] = {
    {4, "PING", CT_Ping},
    {4, "INFO", CT_Info},
    {4, "KEYS", CT_Keys},
};

const size_t num_array_lookups =
    sizeof array_cmd_lookups / sizeof array_cmd_lookups[0];
const size_t num_string_lookups =
    sizeof string_cmd_lookups / sizeof string_cmd_lookups[0];

static cmd parser_parse_cmd(parser* p);
static cmd parser_parse_cmd_v1(parser* p);
static cmd parser_parse_array_cmd(parser* p);
static cmd parser_parse_bulk_string_cmd(parser* p);
static cmd parser_parse_simple_string_cmd(parser* p);
static cmd_type parser_parse_cmd_type(parser* p);
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
static int parser_at_end(parser* p);

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

cmd parse_cmd(parser* p) { return parser_parse_cmd(p); }

static cmd parser_parse_cmd(parser* p) {
    cmd cmd = {0};
    switch (p->version) {
    case 1:
        return parser_parse_cmd_v1(p);
    default:
        p->error = vstr_format("unknown protocol version: %d\n", p->version);
        p->has_error = 1;
        break;
    }
    return cmd;
}

static cmd parser_parse_cmd_v1(parser* p) {
    cmd cmd = {0};
    switch (p->byte) {
    case ARRAY_BYTE:
        return parser_parse_array_cmd(p);
    case BULK_STRING_BYTE:
        return parser_parse_bulk_string_cmd(p);
    case SIMPLE_STRING_BYTE:
        return parser_parse_simple_string_cmd(p);
    default:
        break;
    }
    p->error = vstr_format("unknown type byte: %c\n", p->byte);
    p->has_error = 1;
    return cmd;
}

static cmd parser_parse_array_cmd(parser* p) {
    cmd cmd = {0};
    cmd_type type;
    size_t len;
    if (!parser_expect_peek_to_be_length(p)) {
        p->error = vstr_format("expected length, got %c", parser_peek_byte(p));
        p->has_error = 1;
        return cmd;
    }
    len = parser_parse_length(p);
    if (p->byte != CR_BYTE) {
        p->error = expected_cr(p->byte);
        p->has_error = 1;
        return cmd;
    }
    if (!parser_expect_peek(p, LF_BYTE)) {
        p->error = expected_lf(parser_peek_byte(p));
        p->has_error = 1;
        return cmd;
    }
    parser_read_byte(p);
    type = parser_parse_cmd_type(p);
    if (p->has_error) {
        return cmd;
    }
    switch (type) {
    case CT_Set: {
        object key, value;
        if (len != 3) {
            p->error = vstr_format("set expects 2 arguments. have %lu", len);
            p->has_error = 1;
            return cmd;
        }
        key = parser_parse_object(p);
        if (p->has_error) {
            return cmd;
        }
        value = parser_parse_object(p);
        if (p->has_error) {
            object_free(&key);
            return cmd;
        }
        if (value.type != OT_String) {
            object to_string = object_to_string(&value);
            object_free(&value);
            value = to_string;
        }
        if (!parser_at_end(p)) {
            p->error = vstr_from(
                "expected to be at end, have more data to parse than expected");
            p->has_error = 1;
            object_free(&key);
            object_free(&value);
            return cmd;
        }
        cmd.cat = C_Write;
        cmd.type = type;
        cmd.proc = set_cmd;
        cmd.data.set.key = key;
        cmd.data.set.value = value;
    } break;
    case CT_Get: {
        object key;
        if (len != 2) {
            p->error = vstr_format("get expects 1 argument. have %lu", len);
            p->has_error = 1;
            return cmd;
        }
        key = parser_parse_object(p);
        if (p->has_error) {
            return cmd;
        }
        if (!parser_at_end(p)) {
            p->error = vstr_from(
                "expected to be at end, have more data to parse than expected");
            p->has_error = 1;
            object_free(&key);
            return cmd;
        }
        cmd.cat = C_Read;
        cmd.type = type;
        cmd.proc = get_cmd;
        cmd.data.get = key;
    } break;
    case CT_Del: {
        object key;
        if (len != 2) {
            p->error = vstr_format("del expects 1 argument. have %lu", len);
            p->has_error = 1;
            return cmd;
        }
        key = parser_parse_object(p);
        if (p->has_error) {
            return cmd;
        }
        if (!parser_at_end(p)) {
            p->error = vstr_from(
                "expected to be at end, have more data to parse than expected");
            p->has_error = 1;
            object_free(&key);
            return cmd;
        }
        cmd.cat = C_Write;
        cmd.type = type;
        cmd.proc = del_cmd;
        cmd.data.del = key;
    } break;
    case CT_Auth: {
        object username;
        object password;
        if (len != 3) {
            p->error =
                vstr_format("auth expects 2 arguments. have %lu", len - 1);
            p->has_error = 1;
            return cmd;
        }
        username = parser_parse_object(p);
        if (p->has_error) {
            object_free(&username);
            return cmd;
        }
        if (username.type != OT_String) {
            p->error = vstr_from("username must be a string");
            p->has_error = 1;
            object_free(&username);
            return cmd;
        }
        password = parser_parse_object(p);
        if (p->has_error) {
            object_free(&password);
            return cmd;
        }
        if (password.type != OT_String) {
            p->error = vstr_from("password must be a string");
            p->has_error = 1;
            object_free(&password);
            return cmd;
        }
        cmd.type = CT_Auth;
        cmd.cat = C_Connection;
        cmd.proc = auth_cmd;
        cmd.data.auth.username = username.data.string;
        cmd.data.auth.password = password.data.string;
    } break;
    default:
        return cmd;
    }
    return cmd;
}

static cmd_type parser_parse_cmd_type(parser* p) {
    cmd_type type = CT_Illegal;
    object cmd_obj = parser_parse_object(p);
    size_t cmd_len;
    const char* cmd_str;
    size_t i;
    if (p->has_error) {
        return type;
    }
    switch (cmd_obj.type) {
    case OT_Null:
        p->error = vstr_from("invalid command: null");
        p->has_error = 1;
        return type;
    case OT_Int:
        p->error = vstr_format("invalid command: %ld", cmd_obj.data.integer);
        p->has_error = 1;
        return type;
    case OT_Double:
        p->error = vstr_format("invalid command: %g", cmd_obj.data.dbl);
        p->has_error = 1;
        return type;
    case OT_Boolean:
        p->error = vstr_format("invalid command: %s",
                               cmd_obj.data.boolean ? "true" : "false");
        p->has_error = 1;
        return type;
    case OT_Error:
        p->error = vstr_format("invalid command: %s", cmd_obj.data.string);
        p->has_error = 1;
        object_free(&cmd_obj);
        return type;
    default:
        break;
    }
    cmd_str = vstr_data(&cmd_obj.data.string);
    cmd_len = vstr_len(&cmd_obj.data.string);
    for (i = 0; i < num_array_lookups; ++i) {
        cmd_type_lookup lookup = array_cmd_lookups[i];
        if (cmd_len != lookup.len) {
            continue;
        }
        if (memcmp(lookup.lookup, cmd_str, cmd_len) != 0) {
            continue;
        }
        object_free(&cmd_obj);
        type = lookup.type;
        return type;
    }
    object_free(&cmd_obj);
    p->error = vstr_format("unknown command: %s", cmd_str);
    p->has_error = 1;
    return type;
}

static cmd parser_parse_bulk_string_cmd(parser* p) {
    cmd cmd = {0};
    size_t i;
    object obj = parser_parse_bulk_string(p);
    const char* cmd_string;
    size_t cmd_len;
    if (p->has_error) {
        return cmd;
    }
    cmd_string = vstr_data(&obj.data.string);
    cmd_len = vstr_len(&obj.data.string);
    if (!parser_at_end(p)) {
        p->error = vstr_from(
            "expected to be at end, have more data to parse than expected");
        p->has_error = 1;
        return cmd;
    }
    for (i = 0; i < num_string_lookups; ++i) {
        cmd_type_lookup lookup = string_cmd_lookups[i];
        if (lookup.len != cmd_len) {
            continue;
        }
        if (memcmp(lookup.lookup, cmd_string, cmd_len) != 0) {
            continue;
        }
        cmd.type = lookup.type;
        object_free(&obj);
        switch (cmd.type) {
        case CT_Ping:
            cmd.cat = C_Connection;
            cmd.proc = ping_cmd;
            break;
        case CT_Info:
            cmd.cat = C_Admin;
            break;
        case CT_Keys:
            cmd.cat = C_Read;
            cmd.proc = keys_cmd;
            break;
        default:
            unreachable();
        }
        return cmd;
    }
    object_free(&obj);
    p->error = vstr_format("unknown command: %s", cmd_string);
    p->has_error = 1;
    return cmd;
}

static cmd parser_parse_simple_string_cmd(parser* p) {
    cmd cmd = {0};
    size_t i;
    object obj = parser_parse_simple_string(p);
    const char* cmd_string = vstr_data(&obj.data.string);
    size_t cmd_len = vstr_len(&obj.data.string);
    if (!parser_at_end(p)) {
        p->error = vstr_from(
            "expected to be at end, have more data to parse than expected");
        p->has_error = 1;
        return cmd;
    }
    for (i = 0; i < num_string_lookups; ++i) {
        cmd_type_lookup lookup = string_cmd_lookups[i];
        if (lookup.len != cmd_len) {
            continue;
        }
        if (memcmp(lookup.lookup, cmd_string, cmd_len) != 0) {
            continue;
        }
        cmd.type = lookup.type;
        object_free(&obj);
        switch (cmd.type) {
        case CT_Ping:
            cmd.cat = C_Connection;
            cmd.proc = ping_cmd;
            break;
        case CT_Info:
            cmd.cat = C_Admin;
            break;
        case CT_Keys:
            cmd.cat = C_Read;
            cmd.proc = keys_cmd;
            break;
        default:
            unreachable();
        }
        return cmd;
    }
    object_free(&obj);
    p->error = vstr_format("unknown command: %s", cmd_string);
    p->has_error = 1;
    return cmd;
}

static object parser_parse_object(parser* p) {
    switch (p->version) {
    case 1:
        return parser_parse_object_v1(p);
    default: {
        vstr err = vstr_format("unknown protocol version: %d\n", p->version);
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
    parser_read_byte(p);
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

static int parser_at_end(parser* p) { return p->byte == 0; }
