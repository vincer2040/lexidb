#include "server.h"
#include "builder.h"
#include "clap.h"
#include "cmd.h"
#include "config.h"
#include "ev.h"
#include "ht.h"
#include "log.h"
#include "networking.h"
#include "object.h"
#include "parser.h"
#include "result.h"
#include "util.h"
#include "vstr.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_ADDRESS "127.0.0.1"
#define DEFAULT_PORT 6969
#define SERVER_BACKLOG 10
#define CLIENT_READ_BUF_CAP 4096

result_t(server, vstr);
result_t(client_ptr, vstr);
result_t(object, void*);

static result(server) server_new(const char* addr, uint16_t port);
static lexidb lexidb_new(void);
static void server_free(server* s);
static void lexidb_free(lexidb* db);

static void server_accept(ev* ev, int fd, void* client_data, int mask);
static void read_from_client(ev* ev, int fd, void* client_data, int mask);
static void write_to_client(ev* ev, int fd, void* client_data, int mask);

static result(client_ptr)
    create_client(int fd, uint32_t addr, uint16_t port, uint16_t flags);

static void execute_cmd(server* s, client* c);
static int execute_set_command(server* s, set_cmd* set);
static object* execute_get_command(server* s, get_cmd* get);
static int execute_del_command(server* s, del_cmd* del);
static int execute_push_command(server* s, push_cmd* push);
static result(object) execute_pop_command(server* s);

static int realloc_client_read_buf(client* c);

static int ht_compare_objects(void* a, void* b);
static int client_compare(void* fdp, void* clientp);

static void client_free(client* client);
static void ht_free_object(void* ptr);
static void client_in_vec_free(void* ptr);

int server_run(int argc, char* argv[]) {
    result(server) rs;
    server s;
    args args;
    cla cla;
    object* addr_obj;
    object* port_obj;
    const char* addr;
    uint16_t port;

    if (create_sigint_handler() == -1) {
        error("failed to create sigint handler (errno: %d) %s\n", errno,
              strerror(errno));
        return 1;
    }

    args = args_new();
    args_add(&args, "address", "-a", "--address",
             "the address the server should listen on", String);
    args_add(&args, "port", "-p", "--port",
             "the port the server should listen on", Int);
    args_add(&args, "loglevel", "-ll", "--loglevel",
             "the amount to log (none, info, debug, verbose)", String);
    cla = parse_cmd_line_args(args, argc, argv);

    if (clap_has_error(&cla)) {
        const char* err = clap_error(&cla);
        printf("clap error: %s\n", err);
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    if (clap_version_requested(&cla)) {
        printf("v%d.%d.%d\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    if (clap_help_requested(&cla)) {
        args_free(args);
        clap_free(&cla);
        return 0;
    }

    addr_obj = args_get(&cla, "address");
    if (addr_obj != NULL) {
        addr = vstr_data(&addr_obj->data.string);
    } else {
        addr = DEFAULT_ADDRESS;
    }

    port_obj = args_get(&cla, "port");
    if (port_obj != NULL) {
        port = port_obj->data.num;
    } else {
        port = DEFAULT_PORT;
    }

    rs = server_new(addr, port);

    args_free(args);
    clap_free(&cla);

    if (rs.type == Err) {
        error("%s\n", vstr_data(&(rs.data.err)));
        vstr_free(&(rs.data.err));
        return 1;
    }

    s = rs.data.ok;

    info("listening on %s:%u\n", DEFAULT_ADDRESS, DEFAULT_PORT);

    ev_await(s.ev);

    info("server shutting down\n");

    server_free(&s);

    return 0;
}

static result(server) server_new(const char* addr, uint16_t port) {
    result(server) result = {0};
    server s = {0};
    int sfd = create_tcp_socket(1);
    ev* ev;
    vstr addr_str;

    if (sfd < 0) {
        result.type = Err;
        result.data.err = vstr_format("failed to create socket (errno: %d) %s",
                                      errno, strerror(errno));
        return result;
    }

    if (tcp_bind(sfd, addr, port) < 0) {
        result.type = Err;
        result.data.err = vstr_format("failed to bind socket (errno: %d) %s",
                                      errno, strerror(errno));
        close(sfd);
        return result;
    }

    if (tcp_listen(sfd, SERVER_BACKLOG) < 0) {
        result.type = Err;
        result.data.err =
            vstr_format("failed to listen on socket (errno: %d) %s", errno,
                        strerror(errno));
        close(sfd);
        return result;
    }

    addr_str = vstr_from(addr);
    s.addr = addr_str;
    s.port = port;

    s.clients = vec_new(sizeof(client*));
    if (s.clients == NULL) {
        result.type = Err;
        result.data.err = vstr_format("failed to allocate for client vec");
        close(sfd);
        return result;
    }

    ev = ev_new(SERVER_BACKLOG);
    if (ev == NULL) {
        result.type = Err;
        result.data.err = vstr_format("failed to allocate memory for ev\n");
        vec_free(s.clients, client_in_vec_free);
        close(sfd);
        return result;
    }

    if (ev_add_event(ev, sfd, EV_READ, server_accept, &s) == -1) {
        result.type = Err;
        result.data.err = vstr_format(
            "failed to add server accept as event_fn to ev (errno: %d) %s\n",
            errno, strerror(errno));
        vec_free(s.clients, client_in_vec_free);
        ev_free(ev);
        close(sfd);
        return result;
    }

    s.pid = getpid();
    s.sfd = sfd;
    s.db = lexidb_new();
    s.ev = ev;
    result.type = Ok;
    result.data.ok = s;
    return result;
}

static lexidb lexidb_new(void) {
    lexidb res = {0};
    res.ht = ht_new(sizeof(object), ht_compare_objects);
    res.vec = vec_new(sizeof(object));
    assert(res.vec != NULL);
    return res;
}

static void server_free(server* s) {
    ev_free(s->ev);
    lexidb_free(&s->db);
    vec_free(s->clients, client_in_vec_free);
    close(s->sfd);
}

static void lexidb_free(lexidb* db) {
    ht_free(&(db->ht), ht_free_object, ht_free_object);
    vec_free(db->vec, ht_free_object);
}

static void server_accept(ev* ev, int fd, void* client_data, int mask) {
    server* s = client_data;
    int cfd;
    struct sockaddr_in addr = {0};
    socklen_t len = 0;
    result(client_ptr) r_client;
    client* client;
    int add;

    cfd = tcp_accept(fd, &addr, &len);
    if (cfd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            warn("accept blocked\n");
            return;
        } else {
            error("accept failed (errno: %d) %s\n", errno, strerror(errno));
            return;
        }
    }

    if (make_socket_nonblocking(cfd) == -1) {
        error("failed to make %d nonblocking\n", cfd);
        return;
    }

    r_client = create_client(cfd, addr.sin_addr.s_addr, addr.sin_port, 0);
    if (r_client.type == Err) {
        error("%s\n", vstr_data(&(r_client.data.err)));
        vstr_free(&(r_client.data.err));
        return;
    }

    client = r_client.data.ok;

    add = vec_push(&(s->clients), &client);
    if (add == -1) {
        error("failed to add client to client vector (errno: %d) %s\n", errno,
              strerror(errno));
        return;
    }

    add = ev_add_event(s->ev, cfd, EV_READ, read_from_client, s);
    if (add == -1) {
        error("failed to add read event for client (errno: %d) %s\n", errno,
              strerror(errno));
        client_free(client);
        return;
    }

    info("new connection: %d\n", fd);
}

static void read_from_client(ev* ev, int fd, void* client_data, int mask) {
    ssize_t read_amt;
    server* s = client_data;
    client* c;
    ssize_t found;
    int add;

    found = vec_find(s->clients, &fd, &c, client_compare);
    if (found == -1) {
        error("failed to find client (fd %d) in client vec\n", fd);
        ev_delete_event(ev, fd, EV_READ);
        close(fd);
        return;
    }

    for (;;) {
        /* it should never be greater than, but just in case */
        if (c->read_pos >= c->read_cap) {
            if (realloc_client_read_buf(c) == -1) {
                error("failed to realloce client buffer, closing connection\n");
                vec_remove_at(s->clients, found, NULL);
                ev_delete_event(ev, fd, mask);
                client_free(c);
                return;
            }
        }

        read_amt =
            read(fd, c->read_buf + c->read_pos, c->read_cap - c->read_pos);

        if (read_amt == 0) {
            info("closing %d\n", fd);
            vec_remove_at(s->clients, found, NULL);
            ev_delete_event(ev, fd, mask);
            client_free(c);
            return;
        }

        if (read_amt == -1) {
            if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
                break;
            } else {
                // todo
                error("failed to read from client (errno: %d) %s\n", errno,
                      strerror(errno));
                return;
            }
        }
        c->read_pos += read_amt;
    }

    debug("received: %s\n", c->read_buf);
    execute_cmd(s, c);

    c->write_buf = (uint8_t*)builder_out(&(c->builder));
    c->write_size = builder_len(&(c->builder));

    add = ev_add_event(ev, fd, EV_WRITE, write_to_client, s);
    if (add == -1) {
        fprintf(stderr, "failed to add write event for %d\n", fd);
        return;
    }

    memset(c->read_buf, 0, c->read_pos);
    c->read_pos = 0;

    return;
}

static void write_to_client(ev* ev, int fd, void* client_data, int mask) {
    server* s = client_data;
    ssize_t bytes_sent;
    client* c;
    ssize_t found = vec_find(s->clients, &fd, &c, client_compare);

    if (found == -1) {
        fprintf(stderr, "failed to find client %d\n", fd);
        ev_delete_event(ev, fd, mask);
        close(fd);
        return;
    }

    bytes_sent = write(fd, c->write_buf, c->write_size);

    if (bytes_sent == -1) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
            fprintf(stderr, "write blocked\n");
            return;
        } else {
            fprintf(stderr, "failed to write to client (errno: %d) %s\n", errno,
                    strerror(errno));
            return;
        }
    }

    if (bytes_sent != c->write_size) {
        // todo
        error("failed to write all bytes to %d\n", fd);
        return;
    }

    ev_delete_event(ev, fd, EV_WRITE);
    builder_reset(&(c->builder));
}

static result(client_ptr)
    create_client(int fd, uint32_t addr, uint16_t port, uint16_t flags) {
    result(client_ptr) res = {0};
    client* c;
    c = calloc(1, sizeof *c);
    if (c == NULL) {
        res.type = Err;
        res.data.err = vstr_from("failed to allocate memory for client");
        return res;
    }
    c->fd = fd;
    c->time_connected = get_time();
    c->addr = addr;
    c->port = port;
    c->read_buf = calloc(CLIENT_READ_BUF_CAP, sizeof(uint8_t));
    if (c->read_buf == NULL) {
        free(c);
        res.type = Err;
        res.data.err =
            vstr_from("failed to allocate memory for client read buf");
        return res;
    }
    c->read_cap = CLIENT_READ_BUF_CAP;
    c->read_pos = 0;
    c->builder = builder_new();
    res.type = Ok;
    res.data.ok = c;
    return res;
}

static void execute_cmd(server* s, client* c) {
    cmd cmd = parse(c->read_buf, c->read_pos);
    switch (cmd.type) {
    case Okc:
        break;
    case Ping:
        builder_add_pong(&(c->builder));
        break;
    case Set: {
        int set_res = execute_set_command(s, &(cmd.data.set));
        if (set_res == -1) {
            builder_add_err(&(c->builder), "failed to set", 13);
            break;
        }
        builder_add_ok(&(c->builder));
    } break;
    case Get: {
        object* obj = execute_get_command(s, &(cmd.data.get));
        if (obj == NULL) {
            builder_add_none(&(c->builder));
            break;
        }
        builder_add_object(&(c->builder), obj);
    } break;
    case Del: {
        int del_res = execute_del_command(s, &(cmd.data.del));
        if (del_res == -1) {
            builder_add_err(&(c->builder), "invalid key", 11);
            break;
        }
        builder_add_ok(&(c->builder));
    } break;
    case Push: {
        int push_res = execute_push_command(s, &(cmd.data.push));
        if (push_res == -1) {
            builder_add_err(&(c->builder), "unable to push", 14);
            break;
        }
        builder_add_ok(&(c->builder));
    } break;
    case Pop: {
        result(object) ro = execute_pop_command(s);
        object obj;
        if (ro.type == Err) {
            builder_add_none(&(c->builder));
            break;
        }
        obj = ro.data.ok;
        builder_add_object(&(c->builder), &obj);
        object_free(&obj);
    } break;
    default:
        builder_add_err(&(c->builder), "Invalid command", 15);
        break;
    }
}

static int execute_set_command(server* s, set_cmd* set) {
    object key = set->key;
    object value = set->value;
    int res = ht_insert(&(s->db.ht), &key, sizeof(object), &value,
                        ht_free_object, ht_free_object);
    return res;
}

static object* execute_get_command(server* s, get_cmd* get) {
    object key = get->key;
    object* res = ht_get(&(s->db.ht), &key, sizeof(object));
    object_free(&key);
    return res;
}

static int execute_del_command(server* s, del_cmd* del) {
    object key = del->key;
    int res = ht_delete(&(s->db.ht), &key, sizeof(object), ht_free_object,
                        ht_free_object);
    object_free(&key);
    return res;
}

static int execute_push_command(server* s, push_cmd* push) {
    object value = push->value;
    int res = vec_push(&(s->db.vec), &value);
    return res;
}

static result(object) execute_pop_command(server* s) {
    result(object) ro = {0};
    object out;
    int pop_res = vec_pop(s->db.vec, &out);
    if (pop_res == -1) {
        ro.type = Err;
        return ro;
    }
    ro.type = Ok;
    ro.data.ok = out;
    return ro;
}

static int realloc_client_read_buf(client* c) {
    void* tmp;
    size_t new_cap = c->read_cap << 1;
    tmp = realloc(c->read_buf, new_cap);
    if (tmp == NULL) {
        return -1;
    }
    c->read_buf = tmp;
    memset(c->read_buf + c->read_pos, 0, new_cap - c->read_pos);
    c->read_cap = new_cap;
    return 0;
}

static int ht_compare_objects(void* a, void* b) {
    object* ao = a;
    object* bo = b;
    return object_cmp(ao, bo);
}

static int client_compare(void* fdp, void* clientp) {
    int fd = *((int*)fdp);
    client* c = *((client**)clientp);
    return fd - c->fd;
}

static void ht_free_object(void* ptr) {
    object* o = ptr;
    object_free(o);
}

static void client_free(client* client) {
    close(client->fd);
    builder_free(&(client->builder));
    free(client->read_buf);
    free(client);
}

static void client_in_vec_free(void* ptr) {
    client* c = *((client**)ptr);
    client_free(c);
}
