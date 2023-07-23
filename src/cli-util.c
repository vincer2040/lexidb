#include "cli-util.h"
#include "sock.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Client client_create(char* addr, uint16_t port) {
    int sfd;
    sfd = create_tcp_socket(0);
    if (sfd < 0) {
        LOG(ERROR "failed to create socket (errno: %d)\n%s\n", errno,
            strerror(errno));
        return -1;
    }

    if (connect_tcp_sock(sfd, addr, port) < 0) {
        LOG(ERROR "failed to connect (errno: %d)\n%s\n", errno,
            strerror(errno));
        close(sfd);
        return -1;
    }

    return sfd;
}

char* get_line(const char* prompt) {
    char* ret;
    size_t ins = 0;
    size_t cap = 150;
    char c;

    ret = calloc(cap, sizeof *ret);
    if (ret == NULL) {
        LOG(ERROR "out of memory\n");
        return NULL;
    }

    printf("%s ", prompt);
    fflush(stdout);

    while ((c = getc(stdin)) != '\n') {
        if (ins == cap) {
            cap <<= 1;
            ret = realloc(ret, sizeof *ret * cap);
            if (ret == NULL) {
                LOG(ERROR "out of memory\n");
                goto err;
            }
            memset(ret + ins, 0, sizeof *ret * (cap - ins));
        }
        ret[ins] = c;
        ins++;
    }

    return ret;

err:
    return NULL;
}
