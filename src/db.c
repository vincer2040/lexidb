#include "builder.h"
#include "server.h"
#include "util.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDR "127.0.0.1"
#define PORT 6969

int main(int argc, char** argv) {
    Builder builder = { 0 };

    if (done_from_args(argc, argv) == 1) {
        return 0;
    }

    new_builder(&builder, 32);

    return 0;
}
