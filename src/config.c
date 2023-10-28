#include "config.h"
#include "ht.h"
#include "objects.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
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

int vec_cmp_fn(void* cmp, void* data) {
    char* arg = cmp;
    ConfigOption* opt = data;
    const char* long_arg = opt->arg;
    const char* short_arg = opt->short_arg;
    size_t long_arg_len = strlen(long_arg);
    size_t short_arg_len = strlen(short_arg);
    size_t arg_len = strlen(arg);

    if ((arg_len == long_arg_len) &&
        strncmp(arg, long_arg, long_arg_len) == 0) {
        return 0;
    }
    if ((arg_len == short_arg_len) &&
        strncmp(arg, short_arg, short_arg_len) == 0) {
        return 0;
    }
    return 1;
}

void config_print_version(Configuration* config, const char* arg) {
    ConfigOption opt = {0};
    int find_res = vec_find(config, (void*)arg, vec_cmp_fn, &opt);
    if (find_res == 0) {
        switch (opt.type) {
        case COT_STRING:
            printf("%s\n", opt.default_value.str);
            break;
        case COT_INT:
            printf("%d\n", opt.default_value.integer);
            break;
        default:
            printf("no version indicated\n");
        }
    } else {
        printf("no version indicated\n");
    }
}

bool is_config_option(char* arg, size_t arg_len, const char* long_arg,
                      size_t long_arg_len, const char* short_arg,
                      size_t short_arg_len) {
    if (arg_len == 9 && strncmp(arg, "--version", 9) == 0) {
        return false;
    }
    if (arg_len == 2 && strncmp(arg, "-v", 2) == 0) {
        return false;
    }
    if ((arg_len == long_arg_len) &&
        strncmp(arg, long_arg, long_arg_len) == 0) {
        return true;
    }
    if ((arg_len == short_arg_len) &&
        strncmp(arg, short_arg, short_arg_len) == 0) {
        return true;
    }
    return false;
}

void ht_free_fn(void* ptr) {
    Object* obj = ptr;
    object_free(obj);
    free(obj);
}

int read_integer(char* value) {
    size_t i = 0;
    int res = 0;

    while (isdigit(value[i])) {
        res = (res * 10) + (value[i] - '0');
        i++;
    }
    return res;
}

Ht* configure(Configuration* config, int argc, char** argv) {
    VecIter iter;
    ConfigOption* cur;
    int i;
    size_t ht_initial_cap = vec_len(config);
    size_t j, len = vec_len(config);
    Ht* ht = ht_new(ht_initial_cap, sizeof(Object));

    if (argc == 1) {
        goto fill;
    }

    if ((argc == 2) && strncmp(argv[1], "--help", 6) == 0) {
        config_print_help(config, argv[0]);
        ht_free(ht, ht_free_fn);
        return NULL;
    }

    if ((argc == 2) && strncmp(argv[1], "-h", 2) == 0) {
        config_print_help(config, argv[0]);
        ht_free(ht, ht_free_fn);
        return NULL;
    }

    if ((argc == 2) && strncmp(argv[1], "--version", 9) == 0) {
        config_print_version(config, "--version");
        ht_free(ht, ht_free_fn);
        return NULL;
    }

    if ((argc == 2) && strncmp(argv[1], "-v", 2) == 0) {
        config_print_version(config, "-v");
        ht_free(ht, ht_free_fn);
        return NULL;
    }

    for (i = 1; i < argc; ++i) {
        char* arg = argv[i];
        iter = vec_iter_new(config, 0);
        cur = iter.cur;
        for (cur = iter.cur; cur != NULL;
             vec_iter_next(&iter), cur = iter.cur) {
            const char* long_arg = cur->arg;
            const char* short_arg = cur->short_arg;
            size_t arg_len = strlen(arg);
            size_t long_arg_len = strlen(long_arg);
            size_t short_arg_len = strlen(short_arg);
            bool valid = is_config_option(arg, arg_len, long_arg, long_arg_len,
                                          short_arg, short_arg_len);
            if (valid) {
                Object obj = {0};
                char* value;
                size_t value_len;

                if (cur->type != COT_NULL) {
                    i++;
                } else {
                    obj = object_new(ONULL, NULL, 0);
                    ht_insert(ht, (uint8_t*)long_arg, long_arg_len, &obj,
                              ht_free_fn);
                    goto next;
                }
                if (i >= argc) {
                    printf("%s requires an argument\n", arg);
                    goto fail;
                }
                value = argv[i];

                value_len = strlen(value);

                if (cur->type == COT_STRING) {
                    obj = object_new(STRING, value, value_len);
                    ht_insert(ht, (uint8_t*)long_arg, long_arg_len, &obj,
                              ht_free_fn);
                } else if (cur->type == COT_INT) {
                    int r = read_integer(value);
                    obj = object_new(OINT64, (void*)((int64_t)r), 0);
                    ht_insert(ht, (uint8_t*)long_arg, long_arg_len, &obj,
                              ht_free_fn);
                }
                goto next;
            }
        }

        printf("unkown option: %s\n", arg);
        config_print_help(config, argv[0]);
        goto fail;

    next:
        continue;
    }

fill:
    iter = vec_iter_new(config, 0);
    for (j = 0; j < len; ++j) {
        cur = iter.cur;
        int insert_res;
        const char* arg = cur->arg;
        size_t arg_len = strlen(arg);
        Object new_obj = {0};
        switch (cur->type) {
        case COT_STRING:
            new_obj = object_new(STRING, cur->default_value.str,
                                 strlen(cur->default_value.str));
            break;
        case COT_INT:
            new_obj = object_new(
                OINT64, ((void*)(int64_t)(cur->default_value.integer)), 0);
            break;
        case COT_NULL:
            new_obj = object_new(ONULL, NULL, 0);
            break;
        }

        insert_res =
            ht_try_insert(ht, (uint8_t*)arg, arg_len, &new_obj, ht_free_fn);
        if (insert_res == -1) {
            /* insertion failed, we have to free the created object */
            object_free(&new_obj);
        }

        vec_iter_next(&iter);
    }

    return ht;

fail:
    ht_free(ht, ht_free_fn);

    return NULL;
}

void config_free(Configuration* config) { vec_free(config, NULL); }

void free_configuration_ht(Ht* ht) { ht_free(ht, ht_free_fn); }
