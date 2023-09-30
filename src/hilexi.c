#include "hilexi.h"
#include "builder.h"
#include "objects.h"
#include "sock.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

HiLexiData hilexi_unpack(HiLexi* l);
void hilexi_print_data(HiLexiData* data);
void hilexi_free_data(HiLexiData* data);

#define HILEXI_MAX_READ 4096

#define LOG_ERR(...)                                                           \
    { fprintf(stderr, __VA_ARGS__); }

void hilexi_print_simple(HiLexiSimpleString simple) {
    switch (simple) {
    case HL_INVSS:
        printf("invalid\n");
        break;
    case HL_OK:
        printf("ok\n");
        break;
    case HL_NONE:
        printf("none\n");
        break;
    case HL_PONG:
        printf("pong\n");
        break;
    }
}

void hilexi_print_arr(Vec* data) {
    size_t i, len, data_size;

    len = data->len;
    data_size = data->data_size;

    for (i = 0; i < len; ++i) {
        HiLexiData* at = ((HiLexiData*)(data->data + (i * data_size)));
        hilexi_print_data(at);
    }
}

HiLexi* hilexi_new(char* addr, uint16_t port) {
    HiLexi* l;

    l = calloc(1, sizeof *l);
    if (l == NULL) {
        LOG_ERR("failed to allocate memory\n");
        return NULL;
    }

    l->read_buf = calloc(HILEXI_MAX_READ, sizeof(uint8_t));
    if (l->read_buf == NULL) {
        LOG_ERR("failed to allocate memory\n");
        free(l);
        return NULL;
    }

    l->read_cap = HILEXI_MAX_READ;

    l->sfd = create_tcp_socket(0); // create blocking socket
    if (l->sfd < 0) {
        LOG_ERR("failed to create tcp socket (errno: %d)\n%s\n", errno,
                strerror(errno));
        free(l);
        return NULL;
    }

    l->addr = parse_addr(addr, strlen(addr));
    l->port = port;

    return l;
}

void vec_free_cb(void* ptr) { hilexi_free_data((HiLexiData*)ptr); }

void hilexi_free_data(HiLexiData* data) {
    if (data->type == HL_ARR) {
        vec_free(data->val.arr, vec_free_cb);
    }
    if (data->type == HL_BULK_STRING) {
        vstr_delete(data->val.string);
    }
    if (data->type == HL_ERR) {
        vstr_delete(data->val.string);
    }
}

HiLexiDataType hilexi_get_type(uint8_t ch) {
    switch (ch) {
    case '*':
        return HL_ARR;
    case '$':
        return HL_BULK_STRING;
    case ':':
        return HL_INT;
    case '+':
        return HL_SIMPLE_STRING;
    case '-':
        return HL_ERR;
    default:
        return HL_INV;
    }
}

vstr hilexi_unpack_err(HiLexi* l) {
    vstr str = vstr_new();
    uint8_t* buf = l->read_buf;
    buf++;

    while (*buf != '\r') {
        str = vstr_push_char(str, *buf);
        buf++;
    }

    l->unpack_pos++;
    l->unpack_pos++;

    return str;
}

vstr hilexi_unpack_bulk(HiLexi* l) {
    vstr str;
    uint8_t* buf = l->read_buf + l->unpack_pos;
    size_t i, len = 0;

    buf++;
    l->unpack_pos++;

    while (*buf != '\r') {
        len = (len * 10) + (*buf - '0');
        buf++;
        l->unpack_pos++;
    }

    str = vstr_new_len(len + 1);

    buf++;
    l->unpack_pos++;
    buf++;
    l->unpack_pos++;

    for (i = 0; i < len; ++i, buf++, l->unpack_pos++) {
        str = vstr_push_char(str, *buf);
    }

    l->unpack_pos++;
    l->unpack_pos++;

    return str;
}

int64_t hilexi_unpack_int(HiLexi* l) {
    int64_t res = 0;
    uint64_t temp = 0;
    uint8_t* buf = l->read_buf + l->unpack_pos;
    size_t i, len = 9;
    uint8_t shift = 56;

    // maybe just replace this with memcpy ?
    for (i = 1; i < len; ++i, shift -= 8, l->unpack_pos++) {
        uint8_t at = buf[i];
        temp |= (uint64_t)(at) << shift;
    }

    // hack for checking if value should be negative
    if (temp <= 0x7fffffffffffffffu) {
        res = temp;
    } else {
        res = -1 - (long long int)(0xffffffffffffffffu - temp);
    }

    l->unpack_pos++;
    l->unpack_pos++;

    return res;
}

HiLexiSimpleString hilexi_unpack_simple(HiLexi* l) {
    if (l->read_pos == 7) {
        if (memcmp(l->read_buf, "+PONG\r\n", 7) == 0) {
            return HL_PONG;
        }
        if (memcmp(l->read_buf, "+NONE\r\n", 7) == 0) {
            return HL_NONE;
        }
    }
    if (l->read_pos == 5) {
        if (memcmp(l->read_buf, "+OK\r\n", 5) == 0) {
            return HL_OK;
        }
    }
    return HL_INVSS;
}

Vec* hilexi_unpack_arr(HiLexi* l) {
    uint8_t* buf = l->read_buf + l->unpack_pos;
    size_t i, len = 0;
    Vec* data;

    buf++;
    l->unpack_pos++;

    while (*buf != '\r') {
        len = (len * 10) + (*buf - '0');
        buf++;
        l->unpack_pos++;
    }

    data = vec_new(len, sizeof(HiLexiData));

    buf++;
    l->unpack_pos++;
    buf++;
    l->unpack_pos++;

    for (i = 0; i < len; ++i) {
        HiLexiData di = hilexi_unpack(l);
        vec_push(&data, &di);
    }

    return data;
}

HiLexiData hilexi_unpack(HiLexi* l) {
    HiLexiData data = {0};
    HiLexiDataType type = hilexi_get_type(*(l->read_buf + l->unpack_pos));

    if (type == HL_ERR) {
        vstr err = hilexi_unpack_err(l);
        data.val.string = err;
    }

    if (type == HL_BULK_STRING) {
        vstr bulk = hilexi_unpack_bulk(l);
        data.val.string = bulk;
    }

    if (type == HL_INT) {
        int64_t i64 = hilexi_unpack_int(l);
        data.val.integer = i64;
    }

    if (type == HL_SIMPLE_STRING) {
        HiLexiSimpleString simple = hilexi_unpack_simple(l);
        data.val.simple = simple;
    }

    if (type == HL_ARR) {
        data.val.arr = hilexi_unpack_arr(l);
    }

    data.type = type;

    return data;
}

static int hilexi_read(HiLexi* l) {
    ssize_t bytes_read;

    l->unpack_pos = 0;

    if (!(l->flags & (1 << 1))) {
        memset(l->read_buf, 0, l->read_cap);
        l->read_pos = 0;
    }

    bytes_read = read(l->sfd, l->read_buf + l->read_pos, HILEXI_MAX_READ);

    if (bytes_read < 0) {
        LOG_ERR("failed to read from client (errno: %d)\n%s\n", errno,
                strerror(errno));
        return -1;
    }

    if (bytes_read == HILEXI_MAX_READ) {
        l->flags ^= (1 << 1);
        l->read_pos += HILEXI_MAX_READ;
        if ((l->read_cap - l->read_pos) < HILEXI_MAX_READ) {
            l->read_cap += HILEXI_MAX_READ;
            l->read_buf = realloc(l->read_buf, l->read_cap);
            memset(l->read_buf + l->read_pos, 0, l->read_cap - l->read_pos);
        }
        return 1;
    }

    l->read_pos += bytes_read;

    return 0;
}

static int hilexi_send(HiLexi* l) {
    ssize_t bytes_written = write(l->sfd, l->write_buf, l->write_len);
    if (bytes_written < 0) {
        LOG_ERR("failed to write to server (errno: %d)\n%s\n", errno,
                strerror(errno));
        free(l->write_buf);
        l->write_buf = NULL;
        l->write_len = 0;
        l->write_pos = 0;
        return -1;
    }

    if (bytes_written < l->write_len) {
        LOG_ERR("failed to write all bytes to server (errno: %d)\n%s\n", errno,
                strerror(errno));
        l->write_pos = bytes_written;
        return -1;
    }

    free(l->write_buf);
    l->write_buf = NULL;
    l->write_pos = 0;
    l->write_len = 0;

    return 0;
}

void hilexi_evaluate_msg(HiLexi* l) {
    size_t i, len;
    uint8_t* buf;
    buf = l->read_buf;
    len = l->read_pos;

    for (i = 0; i < len; ++i) {
        printf("%x ", buf[i]);
    }
    printf("\n");
}

void hilexi_print_data(HiLexiData* data) {
    if (data->type == HL_ARR) {
        hilexi_print_arr(data->val.arr);
    }
    if (data->type == HL_BULK_STRING) {
        printf("\"%s\"\n", data->val.string);
    }
    if (data->type == HL_SIMPLE_STRING) {
        hilexi_print_simple(data->val.simple);
    }
    if (data->type == HL_INT) {
        printf("(int) %ld\n", data->val.integer);
    }
    if (data->type == HL_ERR) {
        printf("\"%s\"\n", data->val.string);
    }
}

int hilexi_set(HiLexi* l, uint8_t* key, size_t key_len, char* value,
               size_t value_len) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 3);
    builder_add_string(&b, "SET", 3);
    builder_add_string(&b, ((char*)key), key_len);
    builder_add_string(&b, value, value_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_set_int(HiLexi* l, uint8_t* key, size_t key_len, int64_t value) {
    Builder b;
    HiLexiData data;
    int sendres, readres;

    b = builder_create(32);
    builder_add_arr(&b, 3);
    builder_add_string(&b, "SET", 3);
    builder_add_string(&b, ((char*)key), key_len);
    builder_add_int(&b, value);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_get(HiLexi* l, uint8_t* key, size_t key_len) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "GET", 3);
    builder_add_string(&b, ((char*)key), key_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_del(HiLexi* l, uint8_t* key, size_t key_len) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "DEL", 3);
    builder_add_string(&b, ((char*)key), key_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_push(HiLexi* l, char* value, size_t value_len) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "PUSH", 4);
    builder_add_string(&b, value, value_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_pop(HiLexi* l) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(9);
    builder_add_string(&b, "POP", 3);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_keys(HiLexi* l) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(10);
    builder_add_string(&b, "KEYS", 4);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_values(HiLexi* l) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(12);
    builder_add_string(&b, "VALUES", 6);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_entries(HiLexi* l) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(12);
    builder_add_string(&b, "ENTRIES", 7);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_push_int(HiLexi* l, int64_t value) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "PUSH", 4);
    builder_add_int(&b, value);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_ping(HiLexi* l) {
    Builder b;
    HiLexiData data;

    int sendres, readres;

    b = builder_create(32);
    builder_add_ping(&b);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);

    return 0;
}

int hilexi_cluster_new(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b;
    int sendres, readres;
    HiLexiData data;

    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.NEW", 11);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    if (readres == 1) {
        printf("client sent 4069 bytes, uh oh\n");
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_set(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len, char* value, size_t value_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 4);
    builder_add_string(&b, "CLUSTER.SET", 11);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_string(&b, ((char*)key), key_len);
    builder_add_string(&b, ((char*)value), value_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_set_int(HiLexi* l, uint8_t* name, size_t name_len,
                           uint8_t* key, size_t key_len, int64_t value) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 4);
    builder_add_string(&b, "CLUSTER.SET", 11);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_string(&b, ((char*)key), key_len);
    builder_add_int(&b, value);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo: send all byes
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_get(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 3);
    builder_add_string(&b, "CLUSTER.GET", 11);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_string(&b, ((char*)key), key_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_del(HiLexi* l, uint8_t* name, size_t name_len, uint8_t* key,
                       size_t key_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 3);
    builder_add_string(&b, "CLUSTER.DEL", 11);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_string(&b, ((char*)key), key_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_push(HiLexi* l, uint8_t* name, size_t name_len, char* value,
                        size_t value_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 3);
    builder_add_string(&b, "CLUSTER.PUSH", 12);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_string(&b, ((char*)value), value_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_push_int(HiLexi* l, uint8_t* name, size_t name_len,
                            int64_t value) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 3);
    builder_add_string(&b, "CLUSTER.PUSH", 12);
    builder_add_string(&b, ((char*)name), name_len);
    builder_add_int(&b, value);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_pop(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.POP", 11);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_drop(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.DROP", 12);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_keys(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.KEYS", 12);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_values(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.VALUES", 14);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_cluster_entries(HiLexi* l, uint8_t* name, size_t name_len) {
    Builder b = builder_create(32);
    int sendres, readres;
    HiLexiData data;
    builder_add_arr(&b, 2);
    builder_add_string(&b, "CLUSTER.ENTRIES", 15);
    builder_add_string(&b, ((char*)name), name_len);

    l->write_buf = builder_out(&b);
    l->write_len = b.ins;
    l->write_pos = 0;

    sendres = hilexi_send(l);
    if (sendres == -1) {
        if (l->write_pos == 0) {
            return -1;
        } else {
            // todo write all
            free(l->write_buf);
            l->write_len = 0;
            l->write_pos = 0;
            return -1;
        }
    }

    readres = hilexi_read(l);
    if (readres == -1) {
        return -1;
    }

    data = hilexi_unpack(l);
    hilexi_print_data(&data);
    hilexi_free_data(&data);
    return 0;
}

int hilexi_connect(HiLexi* l) {
    if (l->flags & 1) {
        return 1;
    }
    int r = connect_tcp_sock_u32(l->sfd, l->addr, l->port);
    l->flags = 1;
    return r;
}

void hilexi_disconnect(HiLexi* l) {
    if (l->sfd != -1) {
        close(l->sfd);
        l->sfd = -1;
    }
}

void hilexi_destory(HiLexi* l) {

    hilexi_disconnect(l);

    if (l->read_buf) {
        free(l->read_buf);
        l->read_buf = NULL;
    }

    l->read_cap = 0;
    l->read_pos = 0;

    if (l->write_buf) {
        free(l->write_buf);
        l->write_buf = NULL;
    }

    l->write_pos = 0;
    l->write_len = 0;

    free(l);
}
