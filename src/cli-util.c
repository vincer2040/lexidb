#include "cli-util.h"
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
