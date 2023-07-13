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

    Builder builder;

    if (done_from_args(argc, argv) == 1) {
        return 0;
    }

    builder = create_builder(32);

    builder_add_arr(&builder, 3);
    builder_add_string(&builder, "SET", 3);
    builder_add_string(&builder, "vince", 5);
    builder_add_string(&builder, "is cool", 7);

    dbgbuf(builder);

    free_builder(&builder);

    return 0;
}
