#include "server.h"
#include "builder.h"
#include "cmd.h"
#include "de.h"
#include "ht.h"
#include "lexer.h"
#include "log.h"
#include "objects.h"
#include "parser.h"
#include "sock.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HT_INITIAL_CAP 32
#define YES_NONBLOCK 1
#define BACKLOG 10
#define MAX_READ 4096
#define CONN_BUF_INIT_CAP 4096

void read_from_client(De* de, int fd, void* client_data, uint32_t flags);

Server* server_create(int sfd) {
    Server* server;

    if (sfd < 0) {
        return NULL;
    }

    server = calloc(1, sizeof *server);
    if (server == NULL) {
        return NULL;
    }

    server->sfd = sfd;

    server->ht = ht_new(HT_INITIAL_CAP);
    if (server->ht == NULL) {
        free(server);
        return NULL;
    }

    return server;
}

Connection* create_connection(uint32_t addr, uint16_t port, Server* server) {
    Connection* conn;

    conn = calloc(1, sizeof *conn);
    if (conn == NULL) {
        return NULL;
    }

    conn->addr = addr;
    conn->port = port;
    conn->server = server;

    conn->read_buf = calloc(CONN_BUF_INIT_CAP, sizeof(uint8_t));
    if (conn->read_buf == NULL) {
        free(conn);
        return NULL;
    }

    conn->read_pos = 0;
    conn->read_cap = CONN_BUF_INIT_CAP;
    conn->flags = 0;

    return conn;
}

void close_client(De* de, Connection* conn, int fd, uint32_t flags) {
    LOG(LOG_CLOSE"fd: %d, addr: %u, port: %u\n", fd, conn->addr,
           conn->port);
    if (conn->read_buf) {
        free(conn->read_buf);
        conn->read_buf = NULL;
    }
    if (conn->write_buf) {
        free(conn->read_buf);
        conn->read_buf = NULL;
    }
    free(conn);
    conn = NULL;
    if (de_del_event(de, fd, flags) == -1) {
        fmt_error("error deleting event\n");
        return;
    }
    close(fd);
}

void server_destroy(Server* server) {
    close(server->sfd);
    ht_free(server->ht);
    server->ht = NULL;
    free(server);
    server = NULL;
}

void free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
    free(obj);
}

void free_int_cb(void* ptr) { free(ptr); }

/**
 * run the command sent to the server
 * the command is valid if it is passed to
 * this function, so there is no checking.
 * all commands are in this function
 * to avoid jumping around the whole file.
 */
void evaluate_cmd(Cmd* cmd, Connection* client) {
    CmdT cmd_type = cmd->type;
    if (cmd_type == CPING) {
        // reply with pong
        Builder builder = builder_create(7);
        builder_add_pong(&builder);
        client->write_buf = builder_out(&builder);
        client->write_size = builder.ins;
        return;
    }
    if (cmd_type == SET) {
        // set key and value in ht
        SetCmd set_cmd = cmd->expression.set;
        Builder builder;
        uint8_t* key = set_cmd.key.value;
        size_t key_len = set_cmd.key.len;
        void* value = set_cmd.value.ptr;
        size_t value_size = set_cmd.value.size;
        Object obj;
        int set_res;
        if (set_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
            set_res = ht_insert(client->server->ht, key, key_len, &obj,
                                sizeof obj, free_cb);
        } else if (set_cmd.value.type == VTINT) {
            obj = object_new(OINT, value, value_size);
            set_res = ht_insert(client->server->ht, key, key_len, &obj,
                                sizeof obj, free_int_cb);
        } else {
            obj = object_new(ONULL, NULL, 0);
            set_res = ht_insert(client->server->ht, key, key_len, &obj,
                                sizeof obj, free_int_cb);
        }
        if (set_res != 0) {
            uint8_t* e = ((uint8_t*)"could not set");
            size_t n = strlen((char*)e);
            builder = builder_create(n + 3);
            builder_add_err(&builder, e, n);
        } else {
            builder = builder_create(5);
            builder_add_ok(&builder);
        }
        client->write_buf = builder_out(&builder);
        client->write_size = builder.ins;
        return;
    }
    if (cmd_type == GET) {
        // get value from ht
        GetCmd get_cmd = cmd->expression.get;
        uint8_t* key = get_cmd.key.value;
        size_t key_len = get_cmd.key.len;
        void* get_res = ht_get(client->server->ht, key, key_len);
        Builder builder;
        if (get_res == NULL) {
            builder = builder_create(7);
            builder_add_none(&builder);
        } else {
            Object* obj = ((Object*)get_res);
            String* str;
            builder = builder_create(32);
            if (obj->type == STRING) {
                char* v;
                size_t len;
                str = obj->data.str;
                v = str->data;
                len = str->len;
                builder_add_string(&builder, v, len);
            } else if (obj->type == OINT) {
                int64_t i;
                i = obj->data.i64;
                builder_add_int(&builder, i);
            } else {
                builder_add_none(&builder);
            }
        }
        client->write_buf = builder_out(&builder);
        client->write_size = builder.ins;
        return;
    }
    if (cmd_type == DEL) {
        // delete key and value in ht
        DelCmd del_cmd = cmd->expression.del;
        Builder builder;
        uint8_t* key = del_cmd.key.value;
        size_t key_len = del_cmd.key.len;
        ht_delete(client->server->ht, key, key_len);
        builder = builder_create(5);
        builder_add_ok(&builder);
        client->write_buf = builder_out(&builder);
        client->write_size = builder.ins;
        return;
    }
}

void evaluate_message(uint8_t* data, size_t len, Connection* client) {
    Lexer l;
    Parser p;
    CmdIR cir;
    Cmd cmd;

    l = lexer_new(data, len);
    p = parser_new(&l);
    cir = parse_cmd(&p);
    cmd = cmd_from_statement(&(cir.stmt));

    if (parser_errors_len(&p)) {
        cmdir_free(&cir);
        parser_free_errors(&p);
        printf("invalid cmd\n");
        client->write_buf = calloc(sizeof(uint8_t), 10);
        if (client->write_buf == NULL) {
            fmt_error("out of memory\n");
            return;
        }
        memcpy(client->write_buf, "-INVALID\r\n", 10);
        client->write_size = 10;
        return;
    }

    evaluate_cmd(&cmd, client);

    cmdir_free(&cir);
    parser_free_errors(&p);
}

void write_to_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_sent;
    Connection* conn;

    conn = ((Connection*)client_data);
    if (conn->write_buf != NULL) {
        size_t write_size = conn->write_size;
        bytes_sent = write(fd, conn->write_buf, write_size);
        if (bytes_sent < 0) {
            fmt_error("failed to write to client\n");
            return;
        }

        if (bytes_sent < write_size) {
            fmt_error("failed to send all bytes\n");
            return;
        }
        free(conn->write_buf);
        conn->write_buf = NULL;
        conn->write_size = 0;
    } else {
        bytes_sent = write(fd, "noop", 4);
        if (bytes_sent < 0) {
            fmt_error("failed to write to client\n");
            return;
        }
        if (bytes_sent < 4) {
            fmt_error("failed to send all bytes\n");
            return;
        }
    }

    conn->flags = 0;
    de_del_event(de, fd, DE_WRITE);
    UNUSED(flags);
}

void read_from_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_read;
    Connection* conn;

    conn = ((Connection*)client_data);

    if (conn->flags == 0) {
        memset(conn->read_buf, 0, conn->read_cap);
        conn->read_pos = 0;
    }

    bytes_read = read(fd, conn->read_buf + conn->read_pos, MAX_READ);

    if (bytes_read < 0) {
        printf("e\n");
        fmt_error("wtf?\n");
        return;
    }

    if (bytes_read == 0) {
        close_client(de, conn, fd, flags);
        return;
    }

    // we have filled the buffer, and the client might have more to send
    // if client sends multiple of exactly 4096 bytes, a write event
    // is not emitted because we delete it every time
    if (bytes_read == 4096) {
        conn->flags = 1;
        conn->read_pos += MAX_READ;
        // TODO: yeah.. this probably isn't the best way to check
        if ((conn->read_cap - conn->read_pos) < 4096) {
            conn->read_cap += MAX_READ;
            conn->read_buf =
                realloc(conn->read_buf, sizeof(uint8_t) * conn->read_cap);
            memset(conn->read_buf + conn->read_pos, 0, MAX_READ);
        }
        return;
    }

    conn->read_pos += bytes_read;

    evaluate_message(conn->read_buf, conn->read_pos, conn);

    de_add_event(de, fd, DE_WRITE, write_to_client, client_data);
}

void server_accept(De* de, int fd, void* client_data, uint32_t flags) {
    Server* s;
    Connection* c;
    int cfd;
    struct sockaddr_in addr = {0};
    socklen_t len = 0;

    s = ((Server*)(client_data));
    if (s->sfd != fd) {
        fmt_error("server_accept called for non sfd event\n");
        return;
    }

    cfd = tcp_accept(fd, &addr, &len);
    if (cfd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            fmt_error("accept blocked\n");
            return;
        } else {
            fmt_error("accept failed\n");
            return;
        }
    }

    if (make_socket_nonblocking(cfd) < 0) {
        fmt_error("failed to make fd %d (addr: %u),  nonblocking\n", cfd,
                  addr.sin_addr.s_addr);
        return;
    }

    LOG(LOG_CONNECTION"fd: %d addr: %u port: %u\n", cfd, addr.sin_addr.s_addr, addr.sin_port);

    c = create_connection(addr.sin_addr.s_addr, addr.sin_port, s);
    de_add_event(de, cfd, DE_READ, read_from_client, c);
}

int server(char* addr_str, uint16_t port) {
    Server* server;
    De* de;
    int sfd;
    uint32_t addr;
    uint8_t add_event_res;

    create_sigint_handler();

    addr = parse_addr(addr_str, strlen(addr_str));
    sfd = create_tcp_socket(YES_NONBLOCK);
    if (sfd < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (set_reuse_addr(sfd) < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (bind_tcp_sock(sfd, addr, port) < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (tcp_listen(sfd, BACKLOG) < 0) {
        SLOG_ERROR;
        return -1;
    }

    server = server_create(sfd);
    if (server == NULL) {
        fmt_error("failed to allocate memory for server\n") close(sfd);
        return -1;
    }

    de = de_create(BACKLOG);
    if (de == NULL) {
        fmt_error("failed to allocate memory for event loop\n");
        return -1;
    }

    add_event_res = de_add_event(de, sfd, DE_READ, server_accept, server);

    if ((add_event_res == 1) || (add_event_res == 2)) {
        free(de);
        server_destroy(server);
        return -1;
    }

    LOG(LOG_INFO"server listening on %s:%u\n", addr_str, port);
    de_await(de);

    server_destroy(server);
    de_free(de);

    return 0;
}
