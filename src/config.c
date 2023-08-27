#include "config.h"
#include <stdio.h>
#include <string.h>

LogLevel determine_loglevel(char* level) {
    size_t level_len;
    level_len = strlen(level);

    if (level_len == 3) {
        if (strncmp(level, "cmd", 3) == 0) {
            return LL_CMD;
        }

        return LL_INV;
    }

    if (level_len == 4) {
        if (strncmp(level, "none", 4) == 0) {
            return LL_NONE;
        }

        if (strncmp(level, "info", 4) == 0) {
            return LL_INFO;
        }

        return LL_INV;
    }

    if (level_len == 5) {
        if (strncmp(level, "debug", 5) == 0) {
            return LL_DEBUG;
        }

        return LL_INV;
    }

    if (level_len == 7) {
        if (strncmp(level, "verbose", 7) == 0) {
            return LL_VERBOSE;
        }

        return LL_INV;
    }

    return LL_INV;
}

void configure_from_args(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--logfile", 9) == 0) {
            i++;
            printf("logfile: %s\n", argv[i]);
        }
        if (strncmp(argv[i], "--lexi.conf", 11) == 0) {
            i++;
            printf("conf: %s\n", argv[i]);
        }
        if (strncmp(argv[i], "--log-level", 10) == 0) {
            ++i;
            printf("log level: %s\n", argv[i]);
        }
    }
}
