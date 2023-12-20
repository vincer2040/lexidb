#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

static uint32_t parse_addr(const char* addr_str);

int create_tcp_socket(int non_blocking) {
    if (non_blocking) {
        return socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
    }
    return socket(AF_INET, SOCK_STREAM, 0);
}

int tcp_bind(int sfd, const char* addr_str, uint16_t port) {
    struct sockaddr_in sa = {0};
    socklen_t sal = sizeof sa;
    uint32_t addr = parse_addr(addr_str);
    if (sfd < 0) {
        return -1;
    }
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(addr);
    return bind(sfd, (struct sockaddr*)(&sa), sal);
}

int tcp_listen(int sfd, int backlog) { return listen(sfd, backlog); }

int tcp_accept(int socket, struct sockaddr_in* addr_in, socklen_t* addr_len) {
    return accept(socket, (struct sockaddr*)addr_in, addr_len);
}

int set_reuse_addr(int sfd) {
    int yes = 1;
    return setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
}

int make_socket_nonblocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    return fcntl(sfd, F_SETFL, flags);
}

static uint32_t parse_addr(const char* addr_str) {
    uint32_t addr = 0;
    uint8_t shift = 24;
    size_t len = strlen(addr_str);
    int8_t t;
    size_t i;
    t = 0;
    for (i = 0; i < len; i++) {
        char at;
        at = addr_str[i];
        if (at == '.') {
            addr ^= (((uint32_t)(t)) << shift);
            t = 0;
            shift -= 8;
            continue;
        }
        t = (t * 10) + (at - '0');
    }
    addr ^= ((uint32_t)(t));
    return addr;
}
