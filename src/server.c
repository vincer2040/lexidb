#include "server.h"
#include "builder.h"
#include "ev.h"
#include "ht.h"
#include "log.h"
#include "networking.h"
#include "object.h"
#include "util.h"
#include "vstr.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SERVER_BACKLOG 10
#define CLIENT_READ_BUF_CAP 4096

typedef struct {
    int sfd;
    ht ht;
    ev* ev;
    vec* clients;
} server;

typedef struct {
    int fd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_size;
    builder builder;
    struct timespec time_connected;
} client;

typedef client* client_ptr;

result_t(server, vstr);
result_t(client_ptr, vstr);

static result(server) server_new(const char* addr, uint16_t port);
static void server_free(server* s);

static void server_accept(ev* ev, int fd, void* client_data, int mask);
static void read_from_client(ev* ev, int fd, void* client_data, int mask);
static void write_to_client(ev* ev, int fd, void* client_data, int mask);

static result(client_ptr)
    create_client(int fd, uint32_t addr, uint16_t port, uint16_t flags);

static int ht_compare_objects(void* a, void* b);
static int client_compare(void* fdp, void* clientp);

static void client_free(client* client);
static void ht_free_object(void* ptr);
static void client_in_vec_free(void* ptr);

int server_run(const char* addr, uint16_t port) {
    result(server) rs;
    server s;

    if (create_sigint_handler() == -1) {
        error("failed to create sigint handler (errno: %d) %s\n", errno,
              strerror(errno));
        return 1;
    }

    rs = server_new(addr, port);

    if (rs.type == Err) {
        error("%s\n", vstr_data(&(rs.data.err)));
        vstr_free(&(rs.data.err));
        return 1;
    }

    s = rs.data.ok;

    info("listening on %s:%u\n", addr, port);

    ev_await(s.ev);

    info("server shutting down\n");

    server_free(&s);

    return 0;
}

static result(server) server_new(const char* addr, uint16_t port) {
    result(server) result = {0};
    server s = {0};
    int sfd = create_tcp_socket(0);
    ev* ev;
    ht ht;

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

    ht = ht_new(sizeof(object), ht_compare_objects);

    s.sfd = sfd;
    s.ht = ht;
    s.ev = ev;
    result.type = Ok;
    result.data.ok = s;
    return result;
}

static void server_free(server* s) {
    ev_free(s->ev);
    ht_free(&(s->ht), ht_free_object, ht_free_object);
    vec_free(s->clients, client_in_vec_free);
    close(s->sfd);
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
    read_amt = read(fd, c->read_buf + c->read_pos, c->read_cap - c->read_pos);
    if (read_amt == -1) {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN)) {
            // todo
            return;
        } else {
            // todo
            error("failed to read from client (errno: %d) %s\n", errno,
                  strerror(errno));
            return;
        }
    }

    if (read_amt == 0) {
        info("closing %d\n", fd);
        vec_remove_at(s->clients, found, NULL);
        ev_delete_event(ev, fd, mask);
        client_free(c);
        return;
    }
    c->read_pos += read_amt;

    printf("received: %s\n", c->read_buf);

    builder_add_ok(&(c->builder));
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
        fprintf(stderr, "failed to write all bytes to %d\n", fd);
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
