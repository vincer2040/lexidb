#include "hilexi.h"
#include "builder.h"
#include "hilexi_parser.h"
#include "objects.h"
#include "sock.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HL_BUILDER_INITIAL_CAP 32
#define HL_READ_MAX 4069
#define HL_CONNECTED_FLAG (1 << 1)

static int hilexi_read(HiLexi* l);
static int hilexi_write(HiLexi* l);
static int hilexi_read_and_write(HiLexi* l);
static HiLexiData hilexi_lex_and_parse(HiLexi* l);

HiLexi* hilexi_new(const char* addr, uint16_t port) {
    HiLexi* l;
    int sfd;
    l = calloc(1, sizeof *l);
    assert(l != NULL);
    l->addr = parse_addr(addr, strlen(addr));
    l->port = port;
    l->builder = builder_create(HL_BUILDER_INITIAL_CAP);
    assert(l->builder.buf != NULL);
    l->read_buf = calloc(HL_READ_MAX, sizeof(uint8_t));
    l->read_cap = HL_READ_MAX;
    assert(l->read_buf != NULL);
    sfd = create_tcp_socket(0);
    if (sfd == -1) {
        hilexi_destory(l);
        return NULL;
    }
    l->sfd = sfd;
    return l;
}

int hilexi_connect(HiLexi* l) {
    int connect_res = connect_tcp_sock_u32(l->sfd, l->addr, l->port);
    if (connect_res == -1) {
        return -1;
    }
    l->flags = HL_CONNECTED_FLAG;
    return 0;
}

HiLexiData hilexi_ping(HiLexi* l) {
    Builder* b = &(l->builder);
    HiLexiData res = {0};
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_ping(b);
    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }
    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_set(HiLexi* l, const char* key, size_t key_len,
                      const char* value, size_t val_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "SET", 3);
    b_res = builder_add_string(b, (char*)key, key_len);
    b_res = builder_add_string(b, (char*)value, val_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_set_int(HiLexi* l, const char* key, size_t key_len,
                          int64_t val) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "SET", 3);
    b_res = builder_add_string(b, (char*)key, key_len);
    b_res = builder_add_int(b, val);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_get(HiLexi* l, const char* key, size_t key_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "GET", 3);
    b_res = builder_add_string(b, (char*)key, key_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_del(HiLexi* l, const char* key, size_t key_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "DEL", 3);
    b_res = builder_add_string(b, (char*)key, key_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_keys(HiLexi* l) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_string(b, "KEYS", 4);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_values(HiLexi* l) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_string(b, "VALUES", 6);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_entries(HiLexi* l) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_string(b, "ENTRIES", 7);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_push(HiLexi* l, const char* val, size_t val_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "PUSH", 4);
    b_res = builder_add_string(b, (char*)val, val_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_push_int(HiLexi* l, int64_t val) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "PUSH", 4);
    b_res = builder_add_int(b, val);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_pop(HiLexi* l) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_string(b, "POP", 3);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_enque(HiLexi* l, const char* val, size_t val_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "ENQUE", 5);
    b_res = builder_add_string(b, (char*)val, val_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_enque_int(HiLexi* l, int64_t val) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "ENQUE", 5);
    b_res = builder_add_int(b, val);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_deque(HiLexi* l) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_string(b, "DEQUE", 5);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_new(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.NEW", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_drop(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.DROP", 12);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_set(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len, const char* val, size_t val_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 4);
    b_res = builder_add_string(b, "CLUSTER.SET", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_string(b, (char*)key, key_len);
    b_res = builder_add_string(b, (char*)val, val_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_set_int(HiLexi* l, const char* cluster_name,
                                  size_t cluster_name_len, const char* key,
                                  size_t key_len, int64_t val) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 4);
    b_res = builder_add_string(b, "CLUSTER.SET", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_string(b, (char*)key, key_len);
    b_res = builder_add_int(b, val);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_get(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "CLUSTER.GET", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_string(b, (char*)key, key_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_del(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len, const char* key,
                              size_t key_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "CLUSTER.DEL", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_string(b, (char*)key, key_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_push(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len, const char* val,
                               size_t val_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "CLUSTER.PUSH", 12);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_string(b, (char*)val, val_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_push_int(HiLexi* l, const char* cluster_name,
                                   size_t cluster_name_len, int64_t val) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 3);
    b_res = builder_add_string(b, "CLUSTER.PUSH", 12);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);
    b_res = builder_add_int(b, val);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_pop(HiLexi* l, const char* cluster_name,
                              size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.POP", 11);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_keys(HiLexi* l, const char* cluster_name,
                               size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.KEYS", 12);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_values(HiLexi* l, const char* cluster_name,
                                 size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.VALUES", 14);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

HiLexiData hilexi_cluster_entries(HiLexi* l, const char* cluster_name,
                                  size_t cluster_name_len) {
    HiLexiData res = {0};
    Builder* b = &(l->builder);
    int b_res;
    int read_write_res;

    builder_reset(b);

    b_res = builder_add_arr(b, 2);
    b_res = builder_add_string(b, "CLUSTER.ENTRIES", 15);
    b_res = builder_add_string(b, (char*)cluster_name, cluster_name_len);

    if (b_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_NO_MEM;
        return res;
    }

    l->write_buf = builder_out(b);
    l->write_len = b->ins;
    read_write_res = hilexi_read_and_write(l);
    if (read_write_res == -1) {
        res.type = HL_ERR;
        res.val.hl_err = HL_IO;
        return res;
    }
    res = hilexi_lex_and_parse(l);
    return res;
}

static HiLexiData hilexi_lex_and_parse(HiLexi* l) {
    HiLexiData res = {0};
    HLLexer lex;
    HLParser p;
    lex = hl_lexer_new(l->read_buf, l->read_pos);
    p = hl_parser_new(&lex);
    res = hl_parse(&p);
    return res;
}

static int hilexi_read(HiLexi* l) {
    ssize_t read_amt;
    l->read_pos = 0;
    memset(l->read_buf, 0, l->read_cap);
    read_amt = read(l->sfd, l->read_buf, l->read_cap);
    if (read_amt == -1) {
        return -1;
    }
    l->read_pos = read_amt;
    return 0;
}

static int hilexi_write(HiLexi* l) {
    ssize_t write_amt;
    l->write_pos = 0;
    write_amt = write(l->sfd, l->write_buf, l->write_len);
    if (write_amt == -1) {
        return -1;
    }
    l->write_pos = write_amt;
    return 0;
}

static int hilexi_read_and_write(HiLexi* l) {
    int send_res = hilexi_write(l);
    if (send_res == -1) {
        return -1;
    }
    int read_res = hilexi_read(l);
    if (read_res == -1) {
        return -1;
    }
    return 0;
}

void hilexi_data_vec_free_cb(void* ptr) { hilexi_data_free((HiLexiData*)ptr); }

void hilexi_data_free(HiLexiData* data) {
    switch (data->type) {
    case HL_ARR:
        vec_free(data->val.arr, hilexi_data_vec_free_cb);
        break;
    case HL_BULK_STRING:
        vstr_delete(data->val.string);
        break;
    case HL_ERR_STR:
        vstr_delete(data->val.string);
        break;
    default:
        break;
    }
}

void hilexi_disconnect(HiLexi* l) {
    if (l->flags) {
        close(l->sfd);
    }
}

void hilexi_destory(HiLexi* l) {
    if (l->read_buf) {
        free(l->read_buf);
    }
    builder_free(&(l->builder));
    hilexi_disconnect(l);
    free(l);
}
