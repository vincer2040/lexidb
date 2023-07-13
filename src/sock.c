#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>

/* create a tcp socket with SOCK_NONBLOCK set on fd */
int create_tcp_socket(int nonblocking) {
    if (nonblocking == 1) {
        return socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
    }
    return socket(AF_INET, SOCK_STREAM, 0);
}

/* convert addr string to network byte order uint32_t */
uint32_t parse_addr(char* addr_str, size_t len) {
    uint32_t addr;
    int8_t t;
    size_t i;
    uint8_t shift;
    addr = 0;
    shift = 24;
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

/* bind the socket to address and port */
int bind_tcp_sock(int socket, uint32_t addr, uint16_t port) {
    struct sockaddr_in sa = {0};
    socklen_t addrlen = sizeof sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(addr);
    sa.sin_port = htons(port);
    return bind(socket, ((struct sockaddr*)(&sa)), addrlen);
}

/* listen with backlog */
int tcp_listen(int socket, int backlog) { return listen(socket, backlog); }

int tcp_accept(int socket, struct sockaddr_in* addr, socklen_t* addr_len) {
    return accept(socket, ((struct sockaddr*)(addr)), addr_len);
}

/* enable reuse of address on socket */
int set_reuse_addr(int fd) {
    int yes = 1;
    int r;
    r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    return r;
}

int make_socket_nonblocking(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return -1;
    flags |= (O_NONBLOCK);
    return fcntl(fd, F_SETFL, flags);
}
