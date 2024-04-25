#include "server.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    int res = server_run(argc, argv);
    if (res == -1) {
        printf("error (errno: %d) %s\n", errno, strerror(errno));
        return 1;
    }
    return 0;
}
