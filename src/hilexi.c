#include "hilexi.h"
#include "builder.h"
#include "networking.h"
#include "parser.h"
#include "result.h"
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

int hilexi_authenticate(hilexi* l, const char* username, const char* password) {
    int add = builder_add_array(&l->builder, 3);
    object obj;
    int res;
    add = builder_add_string(&l->builder, "AUTH", 4);
    if (add == -1) {
        return -1;
    }
    add = builder_add_string(&l->builder, username, strlen(username));
    if (add == -1) {
        return -1;
    }
    add = builder_add_string(&l->builder, password, strlen(password));
    if (add == -1) {
        return -1;
    }
    if (hilexi_write(l) == -1) {
        return -1;
    }
    if (hilexi_read(l) == -1) {
        return -1;
    }
    obj = hilexi_parse(l);
    if (obj.type == String) {
        vstr s = obj.data.string;
        vstr ok = vstr_from("OK");
        res = vstr_cmp(&s, &ok);
    } else {
        res = -1;
    }
    object_free(&obj);
    return res;
}

result(object) hilexi_ping(hilexi* l) {
    result(object) res = {0};
    object obj;
    int add = builder_add_ping(&(l->builder));
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add ping to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_info(hilexi* l) {
    result(object) res = {0};
    object obj;
    int add = builder_add_string(&(l->builder), "INFO", 4);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add ping to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_set(hilexi* l, object* key, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&(l->builder), 3);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&(l->builder), "SET", 3);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add set to builder");
        return res;
    }
    add = builder_add_object(&(l->builder), key);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add key to builder");
        return res;
    }
    add = builder_add_object(&(l->builder), value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_get(hilexi* l, object* key) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&(l->builder), 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&(l->builder), "GET", 3);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add get to builder");
        return res;
    }
    add = builder_add_object(&(l->builder), key);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add key to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_del(hilexi* l, object* key) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&(l->builder), 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&(l->builder), "DEL", 3);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add del to builder");
        return res;
    }
    add = builder_add_object(&(l->builder), key);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add key to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_push(hilexi* l, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&(l->builder), 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&(l->builder), "PUSH", 4);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add push to builder");
        return res;
    }
    add = builder_add_object(&(l->builder), value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_pop(hilexi* l) {
    result(object) res = {0};
    object obj;
    int add = builder_add_string(&(l->builder), "POP", 3);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add pop to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno: %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_enque(hilexi* l, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&l->builder, 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&l->builder, "ENQUE", 5);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add enque to builder");
        return res;
    }
    add = builder_add_object(&l->builder, value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_deque(hilexi* l) {
    result(object) res = {0};
    object obj;
    int add = builder_add_string(&l->builder, "DEQUE", 5);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add deque to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_zset(hilexi* l, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&l->builder, 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&l->builder, "ZSET", 4);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add zset to builer");
        return res;
    }
    add = builder_add_object(&l->builder, value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_zhas(hilexi* l, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&l->builder, 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&l->builder, "ZHAS", 4);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add zhas to builer");
        return res;
    }
    add = builder_add_object(&l->builder, value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
}

result(object) hilexi_zdel(hilexi* l, object* value) {
    result(object) res = {0};
    object obj;
    int add = builder_add_array(&l->builder, 2);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add array to builder");
        return res;
    }
    add = builder_add_string(&l->builder, "ZDEL", 4);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add zdel to builer");
        return res;
    }
    add = builder_add_object(&l->builder, value);
    if (add == -1) {
        res.type = Err;
        res.data.err = vstr_from("failed to add value to builder");
        return res;
    }
    if (hilexi_write(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to write to server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    if (hilexi_read(l) == -1) {
        res.type = Err;
        res.data.err = vstr_format("failed to read from server (errno %d) %s",
                                   errno, strerror(errno));
        return res;
    }
    obj = hilexi_parse(l);
    res.type = Ok;
    res.data.ok = obj;
    return res;
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
