#define _POSIX_C_SOURCE 200112L
#include "vnet.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int vnet_make_socket_nonblocking(int socket);

// static int vnet_create_socket(int domain) {
//     return socket(domain, SOCK_STREAM, 0);
// }

static int vnet_create_nonblocking_socket(int domain, int type, int protocol) {
    return socket(domain, type | SOCK_NONBLOCK, protocol);
}

static int vnet_generic_accept(int socket, struct sockaddr* addr,
                               socklen_t* len) {
    int fd;
    do {
        fd = accept(socket, addr, len);
    } while (fd == -1 && errno != EINTR);
    if (fd == -1) {
        return VNET_ERR;
    }
    vnet_make_socket_nonblocking(fd);
    return fd;
}

static int vnet_make_socket_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return VNET_ERR;
    }
    flags |= O_NONBLOCK;
    return fcntl(socket, F_SETFL, flags);
}

static int vnet_set_reuse_addr(int socket) {
    int yes = 1;
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
}

static int vnet_listen(int socket, struct sockaddr* sa, socklen_t len,
                       int backlog) {
    if (bind(socket, sa, len) == -1) {
        return VNET_ERR;
    }
    if (listen(socket, backlog) == -1) {
        return VNET_ERR;
    }
    return VNET_OK;
}

static int vnet_generic_tcp_server(const char* addr, uint16_t port, int family,
                                   int backlog) {
    int res = -1, s;
    char port_str[6] = {0};
    struct addrinfo hints, *info, *p;

    snprintf(port_str, 6, "%u", port);
    memset(&hints, 0, sizeof hints);

    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (addr && (strncmp("*", addr, 1) == 0)) {
        addr = NULL;
    }
    if (addr && (family == AF_INET6) && (strncmp("::*", addr, 3)) == 0) {
        addr = NULL;
    }

    s = getaddrinfo(addr, port_str, &hints, &info);
    if (s == -1) {
        return VNET_ERR;
    }

    for (p = info; p != NULL; p = p->ai_next) {
        res = vnet_create_nonblocking_socket(p->ai_family, p->ai_socktype,
                                             p->ai_protocol);
        if (res == -1) {
            continue;
        }
        if (vnet_set_reuse_addr(res) == -1) {
            close(res);
            freeaddrinfo(info);
            return VNET_ERR;
        }
        if (vnet_listen(res, p->ai_addr, p->ai_addrlen, backlog) == VNET_ERR) {
            close(res);
            freeaddrinfo(info);
            return VNET_ERR;
        }
        goto done;
    }

    if (p == NULL) {
        freeaddrinfo(info);
        return VNET_ERR;
    }

done:
    freeaddrinfo(info);
    return res;
}

int vnet_tcp_server(const char* addr, uint16_t port, int backlog) {
    return vnet_generic_tcp_server(addr, port, AF_INET, backlog);
}

int vnet_tcp6_server(char* addr, uint16_t port, int backlog) {
    return vnet_generic_tcp_server(addr, port, AF_INET6, backlog);
}

int vnet_accept(int socket, char* ip, size_t ip_len, int* port) {
    struct sockaddr_storage sa = {0};
    socklen_t len = sizeof sa;
    int fd = vnet_generic_accept(socket, (struct sockaddr*)(&sa), &len);
    if (fd == -1) {
        return VNET_ERR;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)(&sa);
        if (ip) {
            inet_ntop(AF_INET, (void*)(&s->sin_addr), ip, ip_len);
        }
        if (port) {
            *port = ntohs(s->sin_port);
        }
    } else {
        struct sockaddr_in6* s = (struct sockaddr_in6*)(&sa);
        if (ip) {
            inet_ntop(AF_INET, (void*)(&s->sin6_addr), ip, ip_len);
        }
        if (port) {
            *port = ntohs(s->sin6_port);
        }
    }
    return fd;
}
