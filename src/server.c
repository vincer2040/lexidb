#include "server.h"
#include "de.h"
#include "sock.h"
#include "ht.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#define HT_INITIAL_CAP 32
#define YES_NONBLOCK 1
#define BACKLOG 10
#define MAX_READ 4096
#define CONN_BUF_INIT_CAP 4096

void read_from_client(De* de, int fd, void* client_data, uint32_t flags);

Server* create_server(int sfd) {
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

void close_client(Connection* conn, int fd) {
    printf("closing client fd: %d, addr: %u, port: %u\n", fd, conn->addr, conn->port);
    free(conn->read_buf);
    conn->read_buf = NULL;
    free(conn);
    conn = NULL;
    close(fd);
}

void server_destroy(Server* server) {
    close(server->sfd);
    ht_free(server->ht);
    server->ht = NULL;
    free(server);
    server = NULL;
}

void log_connection(int fd, uint32_t addr, uint16_t port) {
    printf("accepted connection fd: %d, addr: %u, port: %u\n", fd, ntohl(addr), ntohs(port));
}

void write_to_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_sent;
    Connection* conn;

    conn = ((Connection*)client_data);
    bytes_sent = write(fd, "hi", 2);

    if (bytes_sent < 2) {
        fmt_error("failed to send all bytes\n");
        return;
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

    if (bytes_read == 0) {
        close_client(conn, fd);
        return;
    }

    if (bytes_read < 0) {
        close_client(conn, fd);
        fmt_error("wtf?\n");
        return;
    }

    if (bytes_read == 4096) {
        conn->flags = 1;
        conn->read_pos += MAX_READ;
        /* TODO: yeah.. this probably isn't the best way to check */
        if ((conn->read_cap - conn->read_pos) < 4096) {
            conn->read_cap += MAX_READ;
            conn->read_buf = realloc(conn->read_buf, sizeof(uint8_t) * conn->read_cap);
            memset(conn->read_buf + conn->read_pos, 0, MAX_READ);
        }
        return;
    }

    conn->read_pos += bytes_read;

    printf("total bytes: %lu\n%s\n", conn->read_pos, conn->read_buf);

    de_add_event(de, fd, DE_WRITE, write_to_client, client_data);
    UNUSED(flags);
}

void server_accept(De* de, int fd, void* client_data, uint32_t flags)
{
    Server* s;
    Connection* c;
    int cfd;
    struct sockaddr_in addr = { 0 };
    socklen_t len = 0;

    s = ((Server*)(client_data));
    if (s->sfd != fd) {
        fmt_error("server_accept called for non sfd event\n");
        return;
    }

    cfd = tcp_accept(fd, &addr, &len);
    if (cfd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
        {
            fmt_error("accept blocked\n");
            return;
        }
    }

    if (make_socket_nonblocking(cfd) < 0) {
        fmt_error("failed to make fd %d (addr: %u),  nonblocking\n", cfd, addr.sin_addr.s_addr);
        return;
    }

    log_connection(cfd, addr.sin_addr.s_addr, addr.sin_port);

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
        LOG_ERROR;
        return -1;
    }

    if (set_reuse_addr(sfd) < 0) {
        LOG_ERROR;
        return -1;
    }

    if (bind_tcp_sock(sfd, addr, port) < 0) {
        LOG_ERROR;
        return -1;
    }

    if (tcp_listen(sfd, BACKLOG) < 0) {
        LOG_ERROR;
        return -1;
    }

    server = create_server(sfd);
    if (server == NULL) {
        fmt_error("failed to allocate memory for server\n")
        close(sfd);
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

    de_await(de);

    server_destroy(server);
    de_free(de);

    return 0;
}
