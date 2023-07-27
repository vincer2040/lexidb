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

LexiDB* lexidb_new() {
    LexiDB* db;
    db = calloc(1, sizeof *db);

    if (db == NULL) {
        return NULL;
    }

    db->ht = ht_new(HT_INITIAL_CAP);
    if (db->ht == NULL) {
        free(db);
        return NULL;
    }

    db->cluster = cluster_new(HT_INITIAL_CAP);
    if (db->cluster == NULL) {
        ht_free(db->ht);
        free(db);
        return NULL;
    }

    db->vec = vec_new(32, sizeof(Object));
    if (db->vec == NULL) {
        ht_free(db->ht);
        cluster_free(db->cluster);
        free(db);
        return NULL;
    }
    return db;
}

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

    server->db = lexidb_new();
    if (server->db == NULL) {
        free(server);
        close(sfd);
        return NULL;
    }

    return server;
}

Connection* connection_create(uint32_t addr, uint16_t port) {
    Connection* conn;

    conn = calloc(1, sizeof *conn);
    if (conn == NULL) {
        return NULL;
    }

    conn->addr = addr;
    conn->port = port;

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

Client* client_create(Connection* conn, LexiDB* db) {
    Client* client;

    client = calloc(1, sizeof *client);
    if (client == NULL) {
        return NULL;
    }

    client->conn = conn;
    client->db = db;
    return client;
}

void free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
    free(obj);
}

void vec_free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
}

void free_int_cb(void* ptr) { free(ptr); }

void connection_close(De* de, Connection* conn, int fd, uint32_t flags) {
    LOG(LOG_CLOSE "fd: %d, addr: %u, port: %u\n", fd, conn->addr, conn->port);
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

void client_close(De* de, Client* client, int fd, uint32_t flags) {
    connection_close(de, client->conn, fd, flags);
    free(client);
}

void lexidb_free(LexiDB* db) {
    ht_free(db->ht);
    cluster_free(db->cluster);
    vec_free(db->vec, vec_free_cb);
    free(db);
}

void server_destroy(Server* server) {
    close(server->sfd);
    lexidb_free(server->db);
    free(server);
    server = NULL;
}

/**
 * run the command sent to the server
 * the command is valid if it is passed to
 * this function, so there is no checking.
 * all commands are in this function
 * to avoid jumping around the whole file.
 */
void evaluate_cmd(Cmd* cmd, Client* client) {
    CmdT cmd_type;
    Connection* conn = client->conn;
    log_cmd(cmd);
    cmd_type = cmd->type;

    if (cmd_type == CPING) {
        Builder builder = builder_create(7);
        builder_add_pong(&builder);
        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == SET) {
        Builder builder;
        Object obj;
        int set_res;
        SetCmd set_cmd;
        uint8_t* key;
        size_t key_len;
        void* value;
        size_t value_size;

        set_cmd = cmd->expression.set;
        key = set_cmd.key.value;
        key_len = set_cmd.key.len;
        value = set_cmd.value.ptr;
        value_size = set_cmd.value.size;

        if (set_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, sizeof obj,
                                free_cb);
        } else if (set_cmd.value.type == VTINT) {
            obj = object_new(OINT, value, value_size);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, sizeof obj,
                                free_int_cb);
        } else {
            obj = object_new(ONULL, NULL, 0);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, sizeof obj,
                                free_int_cb);
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

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == GET) {
        GetCmd get_cmd;
        uint8_t* key;
        size_t key_len;
        void* get_res;
        Builder builder;

        get_cmd = cmd->expression.get;
        key = get_cmd.key.value;
        key_len = get_cmd.key.len;
        get_res = ht_get(client->db->ht, key, key_len);

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

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == DEL) {
        DelCmd del_cmd;
        Builder builder;
        uint8_t* key;
        size_t key_len;

        del_cmd = cmd->expression.del;
        key = del_cmd.key.value;
        key_len = del_cmd.key.len;

        ht_delete(client->db->ht, key, key_len);

        builder = builder_create(5);
        builder_add_ok(&builder);
        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == PUSH) {
        Object obj;
        PushCmd push_cmd;
        Builder builder;
        int push_res;
        void* value;
        size_t value_size;

        push_cmd = cmd->expression.push;
        value = push_cmd.value.ptr;
        value_size = push_cmd.value.size;

        if (push_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
        } else if (push_cmd.value.type == VTINT) {
            obj = object_new(OINT, value, value_size);
        } else {
            obj = object_new(ONULL, NULL, 0);
        }

        push_res = vec_push(&(client->db->vec), &obj);
        if (push_res != 0) {
            uint8_t* e = ((uint8_t*)"could not push");
            size_t n = strlen((char*)e);
            builder = builder_create(n + 3);
            builder_add_err(&builder, e, n);
        } else {
            builder = builder_create(5);
            builder_add_ok(&builder);
        }

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == POP) {
        Object obj;
        int pop_res;
        Builder builder;

        pop_res = vec_pop(client->db->vec, &obj);
        if (pop_res == -1) {
            builder = builder_create(7);
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        if (obj.type == ONULL) {
            builder = builder_create(7);
            builder_add_none(&builder);
        }

        if (obj.type == OINT) {
            builder = builder_create(11);
            builder_add_int(&builder, obj.data.i64);
        }

        if (obj.type == STRING) {
            builder = builder_create(32);
            builder_add_string(&builder, obj.data.str->data, obj.data.str->len);
            string_free(obj.data.str);
        }

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;
        return;
    }

    if (cmd_type == KEYS) {
        Builder builder;
        HtEntriesIter* iter;
        Entry* cur;
        builder = builder_create(32);

        if (client->db->ht->len == 0) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        builder_add_arr(&builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL; ht_entries_next(iter), cur = iter->cur) {
            builder_add_string(&builder, ((char*)cur->key), cur->key_len);
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;

        return;
    }

    if (cmd_type == VALUES) {
        Builder builder;
        HtEntriesIter* iter;
        Entry* cur;
        builder = builder_create(32);

        if (client->db->ht->len == 0) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        builder_add_arr(&builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL; ht_entries_next(iter), cur = iter->cur) {
            Object* obj = ((Object*)cur->value);
            if (obj->type == OINT) {
                builder_add_int(&builder, obj->data.i64);
            } else if (obj->type == STRING) {
                builder_add_string(&builder, obj->data.str->data, obj->data.str->len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;

        return;
    }

    if (cmd_type == ENTRIES) {
        Builder builder;
        HtEntriesIter* iter;
        Entry* cur;
        builder = builder_create(32);

        if (client->db->ht->len == 0) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(&builder);
            conn->write_buf = builder_out(&builder);
            conn->write_size = builder.ins;
            return;
        }

        builder_add_arr(&builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL; ht_entries_next(iter), cur = iter->cur) {
            uint8_t* key = cur->key;
            size_t key_len = cur->key_len;
            Object* obj = ((Object*)cur->value);
            builder_add_arr(&builder, 2);
            builder_add_string(&builder, ((char*)key), key_len);
            if (obj->type == OINT) {
                builder_add_int(&builder, obj->data.i64);
            } else if (obj->type == STRING) {
                builder_add_string(&builder, obj->data.str->data, obj->data.str->len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(&builder);
        conn->write_size = builder.ins;

        return;
    }
}

void evaluate_message(uint8_t* data, size_t len, Client* client) {
    Lexer l;
    Parser p;
    CmdIR cir;
    Cmd cmd;
    Connection* conn = client->conn;

    slowlog(data, len);

    l = lexer_new(data, len);
    p = parser_new(&l);
    cir = parse_cmd(&p);
    cmd = cmd_from_statement(&(cir.stmt));

    if (parser_errors_len(&p)) {
        cmdir_free(&cir);
        parser_free_errors(&p);
        printf("invalid cmd\n");
        conn->write_buf = calloc(sizeof(uint8_t), 10);
        if (conn->write_buf == NULL) {
            fmt_error("out of memory\n");
            return;
        }
        memcpy(conn->write_buf, "-INVALID\r\n", 10);
        conn->write_size = 10;
        return;
    }

    evaluate_cmd(&cmd, client);

    cmdir_free(&cir);
    parser_free_errors(&p);
}

void write_to_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_sent;
    Client* client;
    Connection* conn;

    client = ((Client*)client_data);
    conn = client->conn;
    if (conn->flags == 1) {
        // we were expecting more data but we now have it all
        evaluate_message(conn->read_buf, conn->read_pos, client);
        conn->flags = 0;
    }
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
        bytes_sent = write(fd, "+noop\r\n", 7);
        if (bytes_sent < 0) {
            fmt_error("failed to write to client\n");
            return;
        }
        if (bytes_sent < 7) {
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
    Client* client;
    Connection* conn;

    client = ((Client*)client_data);
    conn = client->conn;

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
        client_close(de, client, fd, flags);
        // connection_close(de, conn, fd, flags);
        return;
    }

    // we have filled the buffer
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
        // if we do not read again before the write event, the message
        // will be evaluated in write_to_client before
        // writing a response
        de_add_event(de, fd, DE_WRITE, write_to_client, client_data);
        return;
    }

    conn->read_pos += bytes_read;

    evaluate_message(conn->read_buf, conn->read_pos, client);
    conn->flags = 0;

    de_add_event(de, fd, DE_WRITE, write_to_client, client_data);
}

void server_accept(De* de, int fd, void* client_data, uint32_t flags) {
    Server* s;
    Connection* c;
    Client* client;
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

    LOG(LOG_CONNECTION "fd: %d addr: %u port: %u\n", cfd, addr.sin_addr.s_addr,
        addr.sin_port);

    c = connection_create(addr.sin_addr.s_addr, addr.sin_port);
    client = client_create(c, s->db);
    de_add_event(de, cfd, DE_READ, read_from_client, client);
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

    LOG(LOG_INFO "server listening on %s:%u\n", addr_str, port);
    de_await(de);

    LOG(LOG_INFO "server shutting down\n");

    server_destroy(server);
    de_free(de);

    return 0;
}
