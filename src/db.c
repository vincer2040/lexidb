#include "util.h"
#include "server.h"

#define ADDR "127.0.0.1"
#define PORT 6969

int main(int argc, char** argv) {

    if (done_from_args(argc, argv) == 1) {
        return 0;
    }

    server(ADDR, PORT);

    return 0;
}
