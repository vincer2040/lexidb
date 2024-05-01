#define _POSIX_C_SOURCE 200112L
#include "hilexi.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define SIMPLE_STRING_TYPE_BYTE '+'
#define BULK_STRING_TYPE_BYTE '$'
#define INT_TYPE_BYTE ':'
#define DBL_TYPE_BYTE ','
#define ARRAY_TYPE_BYTE '*'
#define SIMPLE_ERROR_TYPE_BYTE '-'
#define BULK_ERROR_TYPE_BYTE '!'
#define NULL_TYPE_BYTE '_'

#define READ_BUF_INITIAL_CAP 4096

void* hilexi_nil = ((void*)"nil");

/* INTERNAL TYPES */

typedef hilexi_string builder;

typedef struct {
    const unsigned char* input;
    size_t len;
    size_t pos;
    int has_error;
    hilexi_string error;
    unsigned char byte;
} parser;

struct hilexi {
    int socket;
    int connected;
    const char* ip;
    uint16_t port;
    unsigned char* read_buf;
    size_t read_buf_pos;
    size_t read_buf_cap;
    unsigned char* write_buf;
    size_t write_buf_len;
    builder builder;
    int has_error;
    hilexi_string error;
};

/* INTERNAL FUNCTION PROTOTYPES */

static int make_socket_nonblocking(int socket);
static hilexi_obj hilexi_parse(hilexi* hl);
static int hilexi_read(hilexi* hl);
static int hilexi_write(hilexi* hl);

static builder builder_new(void);
static size_t builder_len(builder b);
static builder builder_add_simple_string(builder b, const char* str,
                                         size_t str_len);
static builder builder_add_bulk_string(builder b, const char* str,
                                       size_t str_len);
// static builder builder_add_int(builder b, int64_t integer);
// static builder builder_add_double(builder b, double dbl);
static builder builder_add_array(builder b, size_t arr_len);
static void builder_reset(builder b);
static void builder_free(builder b);

static parser parser_new(const unsigned char* input, size_t len);
static hilexi_obj parse(parser* p);

static hilexi_array* hilexi_array_new(void);
static int hilexi_array_push(hilexi_array** arr, const hilexi_obj* obj);

static hilexi_string hilexi_string_new(void);
static hilexi_string hilexi_string_new_len(size_t len);
static hilexi_string hilexi_string_from(const char* cstr);
static hilexi_string hilexi_string_format(const char* fmt, ...);
static hilexi_string hilexi_string_push_char(hilexi_string str, char c);
static hilexi_string hilexi_string_push_string_len(hilexi_string str,
                                                   const char* cstr,
                                                   size_t cstr_len);
static void hilexi_string_reset(hilexi_string str);

/* HILEXI IMPLEMENTATION */

hilexi* hilexi_new(const char* ip, uint16_t port) {
    hilexi* hl = calloc(1, sizeof *hl);
    if (hl == NULL) {
        return NULL;
    }
    hl->ip = ip;
    hl->port = port;
    hl->read_buf = calloc(READ_BUF_INITIAL_CAP, sizeof(unsigned char));
    if (hl->read_buf == NULL) {
        free(hl);
        return NULL;
    }
    hl->read_buf_cap = READ_BUF_INITIAL_CAP;
    hl->socket = -1;
    hl->builder = builder_new();
    if (hl->builder == NULL) {
        free(hl->read_buf);
        free(hl);
        return NULL;
    }
    return hl;
}

int hilexi_has_error(hilexi* hl) { return hl->has_error; }

hilexi_string hilexi_get_error(hilexi* hl) { return hl->error; }

void hilexi_reset_error(hilexi* hl) {
    if (!hl->has_error) {
        return;
    }
    hilexi_string_free(hl->error);
    hl->has_error = 0;
}

int hilexi_connnect(hilexi* hl) {
    int r = -1, s = -1, rv;
    char port_str[6] = {0};
    struct addrinfo hints, *info, *p;
    if (hl->connected) {
        return 0;
    }
    snprintf(port_str, 6, "%u", hl->port);
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    rv = getaddrinfo(hl->ip, port_str, &hints, &info);
    if (rv == -1) {
        goto err;
    }
    for (p = info; p != NULL; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == -1) {
            continue;
        }
        if (make_socket_nonblocking(s) == -1) {
            goto err;
        }
        do {
            r = connect(s, p->ai_addr, p->ai_addrlen);
            if (r == -1) {
                if (errno == EINPROGRESS) {
                    continue;
                } else {
                    goto err;
                }
            }
        } while (r == -1 && errno != EINTR);
        hl->socket = s;
        break;
    }
    freeaddrinfo(info);
    hl->connected = 1;
    return 0;

err:
    if (s != -1) {
        close(s);
    }
    freeaddrinfo(info);
    return -1;
}

hilexi_string hilexi_ping(hilexi* hl) {
    hilexi_obj obj = {0};
    int r;
    builder_reset(hl->builder);
    hl->builder = builder_add_simple_string(hl->builder, "PING", 4);
    if (hl->builder == NULL) {
        hl->has_error = 1;
        hl->error = hilexi_string_from("oom");
        return hilexi_nil;
    }
    hl->write_buf = (unsigned char*)hl->builder;
    hl->write_buf_len = builder_len(hl->builder);
    r = hilexi_write(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error writing (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
    }
    r = hilexi_read(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error reading (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
    }
    obj = hilexi_parse(hl);
    if (obj.type == HLOT_Null) {
        return hilexi_nil;
    }
    if (obj.type == HLOT_Error) {
        return obj.data.string;
    }
    if (obj.type != HLOT_String) {
        hl->has_error = 1;
        hl->error = hilexi_string_from("unknown type from server");
        return hilexi_nil;
    }
    return obj.data.string;
}

hilexi_string hilexi_set(hilexi* hl, const char* key, const char* value) {
    hilexi_obj obj = {0};
    int r;
    builder_reset(hl->builder);
    hl->builder = builder_add_array(hl->builder, 3);
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->builder = builder_add_simple_string(hl->builder, "SET", 3);
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->builder = builder_add_bulk_string(hl->builder, key, strlen(key));
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->builder = builder_add_bulk_string(hl->builder, value, strlen(value));
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->write_buf = (unsigned char*)hl->builder;
    hl->write_buf_len = builder_len(hl->builder);
    r = hilexi_write(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error writing (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
    }
    r = hilexi_read(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error reading (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
        ;
    }
    obj = hilexi_parse(hl);
    if (obj.type == HLOT_Error) {
        return obj.data.string;
    }
    if (obj.type != HLOT_String) {
        hl->has_error = 1;
        hl->error = hilexi_string_from("unknown type from server");
        return hilexi_nil;
    }
    return obj.data.string;

oom:
    hl->has_error = 1;
    hl->error = hilexi_string_from("oom");
    return hilexi_nil;
}

hilexi_string hilexi_get(hilexi* hl, const char* key) {
    hilexi_obj obj = {0};
    int r;
    builder_reset(hl->builder);
    hl->builder = builder_add_array(hl->builder, 2);
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->builder = builder_add_simple_string(hl->builder, "GET", 3);
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->builder = builder_add_bulk_string(hl->builder, key, strlen(key));
    if (hl->builder == NULL) {
        goto oom;
    }
    hl->write_buf = (unsigned char*)hl->builder;
    hl->write_buf_len = builder_len(hl->builder);
    r = hilexi_write(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error writing (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
    }
    r = hilexi_read(hl);
    if (r == -1) {
        hl->has_error = 1;
        hl->error = hilexi_string_format("error reading (errno %d) %s", errno,
                                         strerror(errno));
        return hilexi_nil;
        ;
    }
    obj = hilexi_parse(hl);
    if (obj.type == HLOT_Error) {
        return obj.data.string;
    }
    if (obj.type == HLOT_Null) {
        return hilexi_nil;
    }
    if (obj.type != HLOT_String) {
        hl->has_error = 1;
        hl->error = hilexi_string_from("unknown type from server");
        return hilexi_nil;
    }
    return obj.data.string;

oom:
    hl->has_error = 1;
    hl->error = hilexi_string_from("oom");
    return hilexi_nil;
}

void hilexi_close(hilexi* hl) {
    free(hl->read_buf);
    builder_free(hl->builder);
    if (hl->socket != -1) {
        close(hl->socket);
    }
    if (hl->has_error) {
        hilexi_string_free(hl->error);
    }
    free(hl);
}

static hilexi_obj hilexi_parse(hilexi* hl) {
    hilexi_obj obj;
    parser p = parser_new(hl->read_buf, hl->read_buf_pos);
    obj = parse(&p);
    if (p.has_error) {
        hl->has_error = 1;
        hl->error = p.error;
    }
    return obj;
}

static int hilexi_read(hilexi* hl) {
    size_t pos, cap;
    int received_data = 0;
    memset(hl->read_buf, 0, hl->read_buf_pos);
    hl->read_buf_pos = 0;
    pos = hl->read_buf_pos;
    cap = hl->read_buf_cap;
    for (;;) {
        ssize_t r;
        if (pos == cap) {
            void* tmp;
            cap <<= 1;
            tmp = realloc(hl->read_buf, cap);
            if (tmp == NULL) {
                return -1;
            }
            hl->read_buf = tmp;
            memset(hl->read_buf + pos, 0, cap - pos);
            hl->read_buf_cap = cap;
        }
        r = read(hl->socket, hl->read_buf + pos, cap - pos);
        if (r == 0) {
            break;
        }
        if (r == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (received_data) {
                    break;
                } else {
                    continue;
                }
            } else {
                return -1;
            }
        }
        pos += r;
        received_data = 1;
    }
    hl->read_buf_pos = pos;
    return 0;
}

static int hilexi_write(hilexi* hl) {
    size_t to_write = hl->write_buf_len;
    size_t amt_written = 0;
    while (amt_written < to_write) {
        ssize_t r = write(hl->socket, hl->write_buf + amt_written,
                          to_write - amt_written);
        if (r == -1) {
            return -1;
        }
        amt_written += r;
    }
    return 0;
}

static int make_socket_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    return fcntl(socket, F_SETFL, flags);
}

/* OBJECT IMPLEMENTATION */

void hilexi_obj_print(const hilexi_obj* obj) {
    switch (obj->type) {
    case HLOT_Null:
        printf("(nil)\n");
        break;
    case HLOT_Error:
        printf("error: %s\n", obj->data.string);
        break;
    case HLOT_Int:
        printf("(integer) %ld\n", obj->data.integer);
        break;
    case HLOT_Double:
        printf("(double) %g\n", obj->data.dbl);
        break;
    case HLOT_String:
        printf("\"%s\"\n", obj->data.string);
        break;
    case HLOT_Array:
        printf("array\n");
        break;
    }
}

void hilexi_obj_free(hilexi_obj* obj) {
    switch (obj->type) {
    case HLOT_Error:
        hilexi_string_free(obj->data.string);
        break;
    case HLOT_String:
        hilexi_string_free(obj->data.string);
        break;
    case HLOT_Array:
        hilexi_array_free(obj->data.array);
        break;
    default:
        break;
    }
}

/* PROTOCOL BUILDER IMPLEMENTATION */

static builder builder_new(void) { return hilexi_string_new(); }

static size_t builder_len(builder b) { return hilexi_string_len(b); }

static builder builder_add_end(builder b) {
    b = hilexi_string_push_char(b, '\r');
    if (b == NULL) {
        return NULL;
    }
    b = hilexi_string_push_char(b, '\n');
    if (b == NULL) {
        return NULL;
    }
    return b;
}

static builder builder_add_simple_string(builder b, const char* str,
                                         size_t str_len) {
    b = hilexi_string_push_char(b, SIMPLE_STRING_TYPE_BYTE);
    if (b == NULL) {
        return NULL;
    }
    b = hilexi_string_push_string_len(b, str, str_len);
    if (b == NULL) {
        return NULL;
    }
    return builder_add_end(b);
}

static builder builder_add_length(builder b, size_t len) {
    hilexi_string slen = hilexi_string_format("%zu", len);
    if (slen == NULL) {
        return NULL;
    }
    b = hilexi_string_push_string_len(b, slen, hilexi_string_len(slen));
    if (b == NULL) {
        return NULL;
    }
    hilexi_string_free(slen);
    return b;
}

static builder builder_add_bulk_string(builder b, const char* str,
                                       size_t str_len) {
    b = hilexi_string_push_char(b, BULK_STRING_TYPE_BYTE);
    if (b == NULL) {
        return NULL;
    }
    b = builder_add_length(b, str_len);
    if (b == NULL) {
        return NULL;
    }
    b = builder_add_end(b);
    if (b == NULL) {
        return NULL;
    }
    b = hilexi_string_push_string_len(b, str, str_len);
    if (b == NULL) {
        return NULL;
    }
    return builder_add_end(b);
}

// static builder builder_add_int(builder b, int64_t integer) {
//     hilexi_string s = hilexi_string_format("%ld", integer);
//     if (s == NULL) {
//         return NULL;
//     }
//     b = hilexi_string_push_char(b, INT_TYPE_BYTE);
//     if (b == NULL) {
//         return NULL;
//     }
//     b = hilexi_string_push_string_len(b, s, hilexi_string_len(s));
//     if (b == NULL) {
//         return NULL;
//     }
//     return builder_add_end(b);
// }
//
// static builder builder_add_double(builder b, double dbl) {
//     hilexi_string s = hilexi_string_format("%g", dbl);
//     if (s == NULL) {
//         return NULL;
//     }
//     b = hilexi_string_push_char(b, DBL_TYPE_BYTE);
//     if (b == NULL) {
//         return NULL;
//     }
//     b = hilexi_string_push_string_len(b, s, hilexi_string_len(s));
//     if (b == NULL) {
//         return NULL;
//     }
//     return builder_add_end(b);
// }

static builder builder_add_array(builder b, size_t arr_len) {
    b = hilexi_string_push_char(b, ARRAY_TYPE_BYTE);
    if (b == NULL) {
        return NULL;
    }
    b = builder_add_length(b, arr_len);
    if (b == NULL) {
        return NULL;
    }
    return builder_add_end(b);
}

static void builder_reset(builder b) { hilexi_string_reset(b); }

static void builder_free(builder b) { hilexi_string_free(b); }

/* PARSER IMPLEMENTATION */

static void parser_read_byte(parser* p) {
    if (p->pos >= p->len) {
        p->byte = 0;
        return;
    }
    p->byte = p->input[p->pos];
    p->pos++;
}

static unsigned char peek(parser* p) {
    if (p->pos >= p->len) {
        return 0;
    }
    return p->input[p->pos];
}

static int expect_peek(parser* p, unsigned char byte) {
    if (peek(p) != byte) {
        return 0;
    }
    parser_read_byte(p);
    return 1;
}

static int expect_end(parser* p) {
    if (p->byte != '\r') {
        return 0;
    }
    if (!expect_peek(p, '\n')) {
        return 0;
    }
    return 1;
}

static parser parser_new(const unsigned char* input, size_t len) {
    parser p = {0};
    p.input = input;
    p.len = len;
    parser_read_byte(&p);
    return p;
}

static hilexi_obj parser_parse_null(parser* p) {
    hilexi_obj obj = {0};
    parser_read_byte(p);
    if (!expect_end(p)) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after null");
    }
    return obj;
}

static hilexi_obj parser_parse_simple_string(parser* p) {
    hilexi_obj obj = {0};
    hilexi_string str = hilexi_string_new();
    parser_read_byte(p);
    while (p->byte != '\r' && p->byte != 0) {
        str = hilexi_string_push_char(str, p->byte);
        parser_read_byte(p);
    }
    if (!expect_end(p)) {
        hilexi_string_free(str);
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after simple string");
        return obj;
    }
    obj.type = HLOT_String;
    obj.data.string = str;
    return obj;
}

static size_t parser_parse_length(parser* p) {
    size_t res = 0;
    while (p->byte != '\r' && p->byte != 0) {
        res = (res * 10) + (p->byte - '0');
        parser_read_byte(p);
    }
    return res;
}

static hilexi_obj parser_parse_bulk_string(parser* p) {
    size_t i, len;
    hilexi_string str;
    hilexi_obj obj = {0};
    parser_read_byte(p);
    len = parser_parse_length(p);
    if (!expect_end(p)) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after length");
        return obj;
    }
    parser_read_byte(p);
    str = hilexi_string_new_len(len + 1);
    for (i = 0; i < len; ++i) {
        str = hilexi_string_push_char(str, p->byte);
        parser_read_byte(p);
    }
    if (!expect_end(p)) {
        hilexi_string_free(str);
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after bulk string");
        return obj;
    }
    obj.type = HLOT_String;
    obj.data.string = str;
    return obj;
}

static hilexi_obj parser_parse_int(parser* p) {
    hilexi_obj obj = {0};
    int64_t num = 0;
    int is_negative = 0;
    parser_read_byte(p);
    is_negative = p->byte == '-';
    if (p->byte == '-' || p->byte == '+') {
        parser_read_byte(p);
    } else if (!(isdigit(p->byte))) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected integer");
        return obj;
    }
    while (isdigit(p->byte)) {
        num = (num * 10) + (p->byte - '0');
        parser_read_byte(p);
    }
    if (!expect_end(p)) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after integer");
        return obj;
    }
    if (is_negative) {
        num = -num;
    }
    obj.type = HLOT_Int;
    obj.data.integer = num;
    parser_read_byte(p);
    return obj;
}

static hilexi_obj parser_parse_double(parser* p) {
    hilexi_obj obj = {0};
    const unsigned char* dbl_start = p->input + p->pos;
    const unsigned char* dbl_end;
    double val;
    while ((p->byte != '\r') && (p->byte != 0)) {
        parser_read_byte(p);
    }
    if (p->byte != '\r') {
        p->error = hilexi_string_from("expected \\r\\n after double");
        p->has_error = 1;
        return obj;
    }
    dbl_end = p->input + (p->pos - 1);
    val = strtod((const char*)dbl_start, (char**)&dbl_end);
    if ((val == 0) || (val == HUGE_VAL) || (val == DBL_MIN)) {
        if (errno == ERANGE) {
            p->error = hilexi_string_from(strerror(errno));
            p->has_error = 1;
            return obj;
        }
    }
    if (*dbl_end != '\r') {
        p->error = hilexi_string_from("expected \\r\\n after double");
        p->has_error = 1;
        return obj;
    }
    if (!expect_peek(p, '\n')) {
        p->error = hilexi_string_from("expected \\r\\n after double");
        p->has_error = 1;
        return obj;
    }
    obj.type = HLOT_Double;
    obj.data.dbl = val;
    parser_read_byte(p);
    return obj;
}

static hilexi_obj parser_parse_simple_error(parser* p) {
    hilexi_obj obj = {0};
    hilexi_string str = hilexi_string_new();
    parser_read_byte(p);
    while (p->byte != '\r' && p->byte != 0) {
        str = hilexi_string_push_char(str, p->byte);
        parser_read_byte(p);
    }
    if (!expect_end(p)) {
        hilexi_string_free(str);
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after simple error");
        return obj;
    }
    obj.type = HLOT_Error;
    obj.data.string = str;
    return obj;
}

static hilexi_obj parser_parse_bulk_error(parser* p) {
    size_t i, len;
    hilexi_string str;
    hilexi_obj obj = {0};
    parser_read_byte(p);
    len = parser_parse_length(p);
    if (!expect_end(p)) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after length");
        return obj;
    }
    parser_read_byte(p);
    str = hilexi_string_new_len(len + 1);
    for (i = 0; i < len; ++i) {
        str = hilexi_string_push_char(str, p->byte);
        parser_read_byte(p);
    }
    if (!expect_end(p)) {
        hilexi_string_free(str);
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after bulk error");
        return obj;
    }
    obj.type = HLOT_Error;
    obj.data.string = str;
    return obj;
}

static hilexi_obj parse_obj(parser* p);

static hilexi_obj parser_parse_array(parser* p) {
    hilexi_obj obj = {0};
    size_t i, len;
    hilexi_array* arr;
    len = parser_parse_length(p);
    if (!expect_end(p)) {
        p->has_error = 1;
        p->error = hilexi_string_from("expected \\r\\n after length");
        return obj;
    }
    parser_read_byte(p);
    arr = hilexi_array_new();
    if (arr == NULL) {
        p->has_error = 1;
        p->error = hilexi_string_from("oom");
        return obj;
    }
    for (i = 0; i < len; ++i) {
        hilexi_obj o = parse_obj(p);
        if (p->has_error) {
            hilexi_array_free(arr);
            return obj;
        }
        hilexi_array_push(&arr, &o);
        parser_read_byte(p);
    }
    obj.type = HLOT_Array;
    obj.data.array = arr;
    return obj;
}

static hilexi_obj parse_obj(parser* p) {
    hilexi_obj obj = {0};
    switch (p->byte) {
    case SIMPLE_STRING_TYPE_BYTE:
        return parser_parse_simple_string(p);
    case BULK_STRING_TYPE_BYTE:
        return parser_parse_bulk_string(p);
    case INT_TYPE_BYTE:
        return parser_parse_int(p);
    case DBL_TYPE_BYTE:
        return parser_parse_double(p);
    case SIMPLE_ERROR_TYPE_BYTE:
        return parser_parse_simple_error(p);
    case BULK_ERROR_TYPE_BYTE:
        return parser_parse_bulk_error(p);
    case ARRAY_TYPE_BYTE:
        return parser_parse_array(p);
    case NULL_TYPE_BYTE:
        return parser_parse_null(p);
    case 0:
        p->has_error = 1;
        p->error = hilexi_string_from("unexpected EOF from server");
        break;
    default:
        p->has_error = 1;
        p->error =
            hilexi_string_format("unknown type byte from server: %c", p->byte);
        break;
    }
    return obj;
}

static hilexi_obj parse(parser* p) { return parse_obj(p); }

/* ARRAY IMPLEMENTATION */

#define HILEXI_ARRAY_INITIAL_CAP 32

struct hilexi_array {
    size_t len;
    size_t cap;
    hilexi_obj objs[];
};

static hilexi_array* hilexi_array_new(void) {
    hilexi_array* res;
    size_t needed =
        (sizeof *res) + (HILEXI_ARRAY_INITIAL_CAP * sizeof(hilexi_obj));
    res = malloc(needed);
    if (res == NULL) {
        return NULL;
    }
    memset(res, 0, needed);
    res->cap = HILEXI_ARRAY_INITIAL_CAP;
    return res;
}

size_t hilexi_array_length(const hilexi_array* arr) { return arr->len; }

static int hilexi_array_push(hilexi_array** arr, const hilexi_obj* obj) {
    size_t len = (*arr)->len, cap = (*arr)->cap;
    if (len == cap) {
        void* tmp;
        cap <<= 1;
        tmp = realloc(*arr, (sizeof **arr) + (cap * sizeof(hilexi_obj)));
        if (tmp == NULL) {
            return -1;
        }
        *arr = tmp;
        memset(&(*arr)->objs[len], 0, (cap - len) * sizeof(hilexi_obj));
        (*arr)->cap = cap;
    }
    (*arr)->objs[len] = *obj;
    (*arr)->len++;
    return 0;
}

const hilexi_obj* hilexi_array_get_at(const hilexi_array* arr, size_t idx) {
    if (idx >= arr->len) {
        return NULL;
    }
    return &arr->objs[idx];
}

void hilexi_array_free(hilexi_array* arr) {
    size_t i, len = arr->len;
    for (i = 0; i < len; ++i) {
        hilexi_obj* obj = &arr->objs[i];
        hilexi_obj_free(obj);
    }
    free(arr);
}

/* STRING IMPLEMENTATION */

#define HILEXI_STRING_OBJ_INITAL_CAP 32

typedef struct {
    size_t len;
    size_t cap;
    char data[];
} hilexi_string_obj;

#define hilexi_string_offset ((uintptr_t)(((hilexi_string_obj*)NULL)->data))

static hilexi_string_obj* hilexi_string_obj_new(void) {
    hilexi_string_obj* obj;
    size_t needed = (sizeof *obj) + HILEXI_STRING_OBJ_INITAL_CAP;
    obj = malloc(needed);
    if (obj == NULL) {
        return NULL;
    }
    memset(obj, 0, needed);
    obj->cap = HILEXI_STRING_OBJ_INITAL_CAP;
    return obj;
}

static int hilexi_string_obj_push_char(hilexi_string_obj** obj, char c) {
    size_t len = (*obj)->len, cap = (*obj)->cap;
    if (len >= (cap - 1)) {
        void* tmp;
        cap <<= 1;
        tmp = realloc(*obj, (sizeof **obj) + cap);
        if (tmp == NULL) {
            return -1;
        }
        *obj = tmp;
        memset((*obj)->data + len, 0, cap - len);
        (*obj)->cap = cap;
    }
    (*obj)->data[len] = c;
    (*obj)->len++;
    return 0;
}

static hilexi_string_obj* hilexi_string_obj_new_len(size_t len) {
    hilexi_string_obj* obj;
    size_t needed = (sizeof *obj) + len;
    obj = malloc(needed);
    if (obj == NULL) {
        return NULL;
    }
    memset(obj, 0, needed);
    obj->cap = len;
    return obj;
}

static int hilexi_string_obj_push_string_len(hilexi_string_obj** obj,
                                             const char* cstr,
                                             size_t cstr_len) {
    size_t len = (*obj)->len, cap = (*obj)->cap;
    if ((len + cstr_len) >= cap - 1) {
        void* tmp;
        cap = (cap << 1) + cstr_len;
        tmp = realloc(*obj, (sizeof **obj) + cap);
        if (tmp == NULL) {
            return -1;
        }
        *obj = tmp;
        (*obj)->cap = cap;
        memset((*obj)->data + len, 0, cap - len);
    }
    memcpy((*obj)->data + len, cstr, cstr_len);
    (*obj)->len += cstr_len;
    return 0;
}

static hilexi_string hilexi_string_new(void) {
    hilexi_string_obj* obj = hilexi_string_obj_new();
    if (obj == NULL) {
        return NULL;
    }
    return obj->data;
}

static hilexi_string hilexi_string_from(const char* cstr) {
    size_t len = strlen(cstr);
    hilexi_string_obj* obj = hilexi_string_obj_new_len(len + 1);
    if (obj == NULL) {
        return NULL;
    }
    memcpy(obj->data, cstr, len);
    return obj->data;
}

static hilexi_string hilexi_string_new_len(size_t len) {
    hilexi_string_obj* obj = hilexi_string_obj_new_len(len);
    if (obj == NULL) {
        return NULL;
    }
    return obj->data;
}

static hilexi_string hilexi_string_format(const char* fmt, ...) {
    int n;
    size_t size = 0;
    char* p = NULL;
    va_list ap;
    hilexi_string_obj* obj;

    va_start(ap, fmt);
    n = vsnprintf(p, size, fmt, ap);
    va_end(ap);

    if (n < 0) {
        return NULL;
    }

    size = ((size_t)n) + 1;
    obj = hilexi_string_obj_new_len(size);
    if (obj == NULL) {
        return NULL;
    }

    va_start(ap, fmt);
    n = vsnprintf(obj->data, size, fmt, ap);
    va_end(ap);

    if (n < 0) {
        free(obj);
        return NULL;
    }

    obj->len = n;

    return obj->data;
}

size_t hilexi_string_len(const hilexi_string str) {
    hilexi_string_obj* obj = (hilexi_string_obj*)(str - hilexi_string_offset);
    return obj->len;
}

static hilexi_string hilexi_string_push_char(hilexi_string str, char c) {
    hilexi_string_obj* obj = (hilexi_string_obj*)(str - hilexi_string_offset);
    int r = hilexi_string_obj_push_char(&obj, c);
    if (r == -1) {
        return NULL;
    }
    return obj->data;
}

static hilexi_string hilexi_string_push_string_len(hilexi_string str,
                                                   const char* cstr,
                                                   size_t cstr_len) {
    hilexi_string_obj* obj = (hilexi_string_obj*)(str - hilexi_string_offset);
    int r = hilexi_string_obj_push_string_len(&obj, cstr, cstr_len);
    if (r == -1) {
        return NULL;
    }
    return obj->data;
}

static void hilexi_string_reset(hilexi_string str) {
    hilexi_string_obj* obj = (hilexi_string_obj*)(str - hilexi_string_offset);
    if (obj->len == 0) {
        return;
    }
    memset(obj->data, 0, obj->len);
    obj->len = 0;
}

void hilexi_string_free(hilexi_string str) {
    hilexi_string_obj* obj = (hilexi_string_obj*)(str - hilexi_string_offset);
    free(obj);
}
