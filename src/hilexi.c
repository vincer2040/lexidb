#include "builder.h"
#include "hilexi.h"
#include "sock.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HILEXI_MAX_READ 4096

#define LOG_ERR(...)                                                           \
    { fprintf(stderr, __VA_ARGS__); }

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

static int hilexi_read(HiLexi* l) {
    ssize_t bytes_read;

    if (!(l->flags & (1<<1))) {
        memset(l->read_buf, 0, l->read_cap);
        l->read_pos = 0;
    }

    bytes_read = read(l->sfd, l->read_buf + l->read_pos, HILEXI_MAX_READ);

    if (bytes_read < 0) {
        LOG_ERR("failed to read from client (errno: %d)\n%s\n", errno, strerror(errno));
        return -1;
    }

    if (bytes_read == HILEXI_MAX_READ) {
        l->flags ^= (1<<1);
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
        LOG_ERR("failed to write to server (errno: %d)\n%s\n", errno, strerror(errno));
        free(l->write_buf);
        l->write_buf = NULL;
        l->write_len = 0;
        l->write_pos = 0;
        return -1;
    }

    if (bytes_written < l->write_len) {
        LOG_ERR("failed to write all bytes to server (errno: %d)\n%s\n", errno, strerror(errno));
        l->write_pos = bytes_written;
        return -1;
    }

    free(l->write_buf);
    l->write_buf = NULL;
    l->write_pos = 0;
    l->write_len = 0;

    return 0;
}

static void hilexi_evaluate_msg(HiLexi* l) {
    size_t i, len;
    uint8_t* buf;
    buf = l->read_buf;
    len = l->read_pos;

    for (i = 0; i < len; ++i) {
        printf("%x ", buf[i]);
    }
    printf("\n");
}

int hilexi_set(HiLexi* l, uint8_t* key, size_t key_len, char* value, size_t value_len) {
    Builder b;
    int sendres, readres;

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

    hilexi_evaluate_msg(l);

    return 0;
}

int hilexi_set_int(HiLexi* l, uint8_t* key, size_t key_len, int64_t value) {
    Builder b;
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

    hilexi_evaluate_msg(l);

    return 0;
}

int hilexi_get(HiLexi* l, uint8_t* key, size_t key_len) {
    Builder b;
    int sendres, readres;

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

    hilexi_evaluate_msg(l);

    return 0;
}

int hilexi_del(HiLexi* l, uint8_t* key, size_t key_len) {
    Builder b;
    int sendres, readres;

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

    hilexi_evaluate_msg(l);

    return 0;
}

int hilexi_ping(HiLexi* l) {
    Builder b;
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

    hilexi_evaluate_msg(l);

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
