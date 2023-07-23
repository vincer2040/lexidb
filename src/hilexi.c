#include "hilexi.h"
#include "sock.h"
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_ERR(...)                                                           \
    { fprintf(stderr, __VA_ARGS__); }

HiLexi* hilexi_new(char* addr, uint16_t port) {
    HiLexi* l;

    l = calloc(1, sizeof *l);
    if (l == NULL) {
        LOG_ERR("failed to allocate memory\n");
        return NULL;
    }

    l->sfd = create_tcp_socket(0); // create blocking socket
    if (l->sfd < 0) {
        LOG_ERR("failed to create tcp socket (errno: %d)\n%s\n", errno, strerror(errno));
        free(l);
        return NULL;
    }

    l->addr = parse_addr(addr, strlen(addr));
    l->port = port;

    return l;
}
