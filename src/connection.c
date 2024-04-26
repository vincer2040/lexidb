#include "server.h"
#include <stdlib.h>

connection* connection_new(int fd) {
    connection* conn = calloc(1, sizeof *conn);
    conn->state = CS_Accepting;
    conn->fd = fd;
    return conn;
}

void connection_close(connection* conn) {
    close(conn->fd);
    free(conn);
}
