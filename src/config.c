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

Configuration* config_new(void) {
    Configuration* config;
    config = vec_new(5, sizeof(ConfigOption));
    return config;
}

int config_add_option(Configuration** config, const char* arg,
                      const char* short_arg, ConfigOptionType type,
                      void* default_value, const char* metadata) {
    ConfigOption opt = {0};
    opt.arg = arg;
    opt.short_arg = short_arg;
    opt.type = type;

    switch (type) {
    case COT_STRING:
        opt.default_value.str = default_value;
        break;
    case COT_INT:
        opt.default_value.integer = *((int*)default_value);
        break;
    case COT_NULL:
        break;
    }

    opt.metadata = metadata;

    return vec_push(config, &opt);
}

void for_each_fn(void* ptr) {
    ConfigOption* opt = ptr;
    printf("\t%s (%s) -> %s\n", opt->arg, opt->short_arg, opt->metadata);
}

void config_print_help(Configuration* config, char* name) {
    printf("%s [options]\n", name);
    printf("options:\n");
    printf("\t--help (-h) -> show help\n");
    vec_for_each(config, for_each_fn);
}

void configure(Configuration* config, int argc, char** argv) {
    if (argc == 1) {
        return;
    }

    if ((argc == 2) && strncmp(argv[1], "--help", strlen("--help")) == 0) {
        config_print_help(config, argv[0]);
        return;
    }

    if ((argc == 2) && strncmp(argv[1], "-h", strlen("-h")) == 0) {
        config_print_help(config, argv[0]);
        return;
    }
}

void config_free(Configuration* config) { vec_free(config, NULL); }
