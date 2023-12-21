#include "hilexi.h"
#include "builder.h"
#include "networking.h"
#include "parser.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define READ_BUF_INITIAL_CAP 4096

static object hilexi_parse(hilexi* l);
static ssize_t hilexi_read(hilexi* l);
static ssize_t hilexi_write(hilexi* l);
static int realloc_read_buf(hilexi* l);

result(hilexi) hilexi_new(const char* addr, uint16_t port) {
    result(hilexi) rl = {0};
    hilexi l = {0};
    int sfd;

    l.addr = parse_addr(addr);
    l.port = port;

    sfd = create_tcp_socket(1);
    if (sfd == -1) {
        rl.type = Err;
        rl.data.err = vstr_format("failed to make socket (errno %d) %s", errno,
                                  strerror(errno));
        return rl;
    }

    l.sfd = sfd;

    l.read_buf = calloc(READ_BUF_INITIAL_CAP, sizeof(uint8_t));
    if (l.read_buf == NULL) {
        rl.type = Err;
        rl.data.err = vstr_format("failed to allocate read buffer");
        close(sfd);
        return rl;
    }

    l.read_pos = 0;
    l.read_cap = READ_BUF_INITIAL_CAP;

    l.builder = builder_new();

    rl.type = Ok;
    rl.data.ok = l;

    return rl;
}

int hilexi_connect(hilexi* l) {
    int res;
    while (1) {
        res = tcp_connect(l->sfd, l->addr, l->port);
        if (res == 0) {
            break;
        } else if (res == -1) {
            if (errno == EINPROGRESS) {
                continue;
            } else {
                break;
            }
        }
    }
    return res;
}

object hilexi_ping(hilexi* l) {
    builder_add_ping(&(l->builder));
    if (hilexi_write(l) == -1) {
        return object_new(Null, NULL);
    }
    if (hilexi_read(l) == -1) {
        return object_new(Null, NULL);
    }
    return hilexi_parse(l);
}

void hilexi_close(hilexi* l) {
    free(l->read_buf);
    close(l->sfd);
    builder_free(&(l->builder));
}

static object hilexi_parse(hilexi* l) {
    object obj = parse_from_server(l->read_buf, l->read_pos);
    memset(l->read_buf, 0, l->read_pos);
    l->read_pos = 0;
    return obj;
}

static ssize_t hilexi_read(hilexi* l) {
    ssize_t amt_read;
    int received_data = 0;
    for (;;) {
        if (l->read_pos == l->read_cap) {
            if (realloc_read_buf(l) == -1) {
                return -1;
            }
        }
        amt_read =
            read(l->sfd, l->read_buf + l->read_pos, l->read_cap - l->read_pos);
        if (amt_read == 0) {
            return 0;
        }
        if (amt_read == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                if (received_data) {
                    break;
                } else {
                    continue;
                }
            }
            return -1;
        }
        received_data = 1;
        l->read_pos += amt_read;
    }

    return l->read_pos;
}

static ssize_t hilexi_write(hilexi* l) {
    const uint8_t* write_buf = builder_out(&(l->builder));
    size_t write_size = builder_len(&(l->builder));
    ssize_t amount_written = 0;
    while (amount_written < write_size) {
        ssize_t amt = write(l->sfd, write_buf + amount_written,
                            write_size - amount_written);
        if (amt == -1) {
            amount_written = -1;
            break;
        }
        amount_written += amt;
    }
    builder_reset(&(l->builder));
    return amount_written;
}

static int realloc_read_buf(hilexi* l) {
    size_t new_cap = l->read_cap << 1;
    void* tmp = realloc(l->read_buf, new_cap);
    if (tmp == NULL) {
        return -1;
    }
    l->read_buf = tmp;
    memset(l->read_buf + l->read_pos, 0, new_cap - l->read_pos);
    l->read_cap = new_cap;
    return 0;
}
