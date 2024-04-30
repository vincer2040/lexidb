#include "ev.h"
#include "server.h"
#include <errno.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>

shared_reply shared_replies = {
    .ok = "+OK\r\n",
    .pong = "+PONG\r\n",
    .none = "+NONE\r\n",
    .denied_cmd = "-ENOACCESS\r\n",
    .null = "_\r\n",
    .zero = ":0\r\n",
    .one = ":1\r\n",
    .invalid_auth = "-EINVALIDAUTH\r\n",
};

#define CLIENT_READ_BUF_INITIAL_CAP 4096

static int find_client_to_remove(const void* cmp, const void* el);
static int client_realloc_read_buf(client* client);

client* client_new(connection* conn) {
    client* client = calloc(1, sizeof *client);
    client->conn = conn;
    client->id = server.next_client_id;
    server.next_client_id++;
    client->conn = conn;
    client_select_db(client, 0);
    client->creation_time = time(NULL);
    client->authenticated = 0;
    client->protocol_version = 1;
    client->read_buf_pos = 0;
    client->read_buf_cap = CLIENT_READ_BUF_INITIAL_CAP;
    client->read_buf =
        calloc(CLIENT_READ_BUF_INITIAL_CAP, sizeof(unsigned char));
    if (client->read_buf == NULL) {
        free(client);
        return NULL;
    }
    client->builder = builder_new();
    client->write_len = 0;
    client->write_buf = NULL;
    return client;
}

int client_read(client* client) {
    ssize_t amt_read;
    for (;;) {
        size_t to_read;
        if (client->read_buf_pos >= client->read_buf_cap) {
            if (client_realloc_read_buf(client) == -1) {
                return -1;
            }
        }
        to_read = client->read_buf_cap - client->read_buf_pos;
        amt_read = read(client->conn->fd,
                        client->read_buf + client->read_buf_pos, to_read);
        if (amt_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            } else {
                client->conn->last_errno = errno;
                client->conn->state = CS_Error;
                return -1;
            }
        }
        if (amt_read == 0) {
            client->conn->state = CS_Closed;
            break;
        }
        client->read_buf_pos += amt_read;
    }
    return 0;
}

int client_write(client* client) {
    ssize_t amt_written;
    size_t bytes_sent = 0, to_send = client->write_len;
    while (bytes_sent < to_send) {
        size_t amt_to_write = to_send - bytes_sent;
        amt_written = write(client->conn->fd, client->write_buf + bytes_sent,
                            amt_to_write);
        if (amt_written == -1) {
            client->conn->last_errno = errno;
            client->conn->state = CS_Error;
            builder_reset(&client->builder);
            return -1;
        }
        bytes_sent += amt_written;
    }
    builder_reset(&client->builder);
    return 0;
}

void client_close(client* client) {
    free(client->read_buf);
    builder_free(&client->builder);
    ev_delete_event(server.ev, client->conn->fd, EV_READ | EV_WRITE);
    vec_remove(server.clients, &client->conn->fd, find_client_to_remove, NULL);
    connection_close(client->conn);
    free(client);
}

void client_add_reply_ok(client* client) {
    client->write_buf = (unsigned char*)shared_replies.ok;
    client->write_len = 5;
}

void client_add_reply_pong(client* client) {
    client->write_buf = (unsigned char*)shared_replies.pong;
    client->write_len = 7;
}

void client_add_reply_none(client* client) {
    client->write_buf = (unsigned char*)shared_replies.none;
    client->write_len = 7;
}

void client_add_reply_no_access(client* client) {
    client->write_buf = (unsigned char*)shared_replies.denied_cmd;
    client->write_len = 12;
}

void client_add_reply_null(client* client) {
    client->write_buf = (unsigned char*)shared_replies.null;
    client->write_len = 3;
}

void client_add_reply_zero(client* client) {
    client->write_buf = (unsigned char*)shared_replies.zero;
    client->write_len = 4;
}

void client_add_reply_one(client* client) {
    client->write_buf = (unsigned char*)shared_replies.one;
    client->write_len = 4;
}

void client_add_reply_invalid_auth(client* client) {
    client->write_buf = (unsigned char*)shared_replies.invalid_auth;
    client->write_len = 15;
}

int client_add_reply_simple_error(client* client, const vstr* error) {
    int res = builder_add_simple_err(&client->builder, vstr_data(error),
                                     vstr_len(error));
    if (res == -1) {
        return -1;
    }
    client->write_buf = builder_out(&client->builder);
    client->write_len = builder_len(&client->builder);
    return 0;
}

int client_add_reply_bulk_error(client* client, const vstr* error) {
    int res = builder_add_bulk_err(&client->builder, vstr_data(error),
                                   vstr_len(error));
    if (res == -1) {
        return -1;
    }
    client->write_buf = builder_out(&client->builder);
    client->write_len = builder_len(&client->builder);
    return 0;
}

int client_add_reply_object(client* client, const object* obj) {
    int res = builder_add_object(&client->builder, obj);
    if (res == -1) {
        return -1;
    }
    client->write_buf = builder_out(&client->builder);
    client->write_len = builder_len(&client->builder);
    return 0;
}

int client_add_reply_array(client* client, size_t len) {
    int res = builder_add_array(&client->builder, len);
    if (res == -1) {
        return -1;
    }
    client->write_buf = builder_out(&client->builder);
    client->write_len = builder_len(&client->builder);
    return 0;
}

int client_add_reply_bulk_string(client* client, const vstr* string) {
    int res = builder_add_bulk_string(&client->builder, vstr_data(string), vstr_len(string));
    if (res == -1) {
        return -1;
    }
    client->write_buf = builder_out(&client->builder);
    client->write_len = builder_len(&client->builder);
    return 0;
}

static int client_realloc_read_buf(client* client) {
    size_t new_cap = client->read_buf_cap << 1;
    void* tmp = realloc(client->read_buf, new_cap);
    if (tmp == NULL) {
        return -1;
    }
    client->read_buf = tmp;
    memset(client->read_buf + client->read_buf_pos, 0,
           new_cap - client->read_buf_pos);
    client->read_buf_cap = new_cap;
    return 0;
}

void client_select_db(client* client, uint64_t db) {
    if (db >= server.num_databases) {
        return;
    }
    client->db = &server.databases[db];
}

static int find_client_to_remove(const void* cmp, const void* el) {
    int fd = *((const int*)cmp);
    const client* c = *((const client**)el);
    if (c->conn->fd == fd) {
        return 0;
    }
    return 1;
}
