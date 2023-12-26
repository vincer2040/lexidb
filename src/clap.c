#include "clap.h"
#include "vstr.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HAS_SHORT (1 << 0)
#define HAS_LONG (1 << 1)
#define HAS_DESCRIPTION (1 << 2)
#define HELP_REQUESTED 1
#define VERSION_REQUESTED 2
#define CLA_ERROR -1

#define max(a, b) a > b ? a : b;

typedef struct {
    vstr name;
    vstr short_arg;
    vstr long_arg;
    vstr description;
    objectt type;
    uint32_t flags;
} arg;

static arg arg_new(const char* name, const char* short_arg,
                   const char* long_arg, const char* description, objectt type);
static int parse_args(cla* cla, args args, int argc, char* argv[]);
static void show_help(char* program_name, args args);
static int64_t parse_int_arg(char* str);
static vstr parse_str_arg(char* str);
static int find_arg_fn(void* arg_str_ptr, void* arg_ptr);
static void arg_free(void* ptr);
static void ht_free_object(void* ptr);

args args_new() {
    vec* v = vec_new(sizeof(arg));
    return v;
}

int args_add(args* args, const char* name, const char* short_arg,
             const char* long_arg, const char* description, objectt type) {
    arg arg;
    if ((short_arg == NULL) && (long_arg == NULL)) {
        return -1;
    }
    arg = arg_new(name, short_arg, long_arg, description, type);
    return vec_push(args, &arg);
}

void args_free(args args) { vec_free(args, arg_free); }

cla parse_cmd_line_args(args args, int argc, char* argv[]) {
    cla cla = {0};
    int parse_res;
    cla.args = ht_new(sizeof(object), NULL);
    parse_res = parse_args(&cla, args, argc, argv);
    if (parse_res == CLA_ERROR) {
        cla.has_error = 1;
        return cla;
    }
    if (parse_res == HELP_REQUESTED) {
        cla.help_requested = 1;
        show_help(argv[0], args);
        return cla;
    }
    if (parse_res == VERSION_REQUESTED) {
        cla.version_requested = 1;
        return cla;
    }
    return cla;
}

object* args_get(cla* cla, const char* name) {
    return ht_get(&cla->args, (void*)name, strlen(name));
}

int clap_has_error(cla* cla) { return cla->has_error; }

const char* clap_error(cla* cla) { return vstr_data(&cla->error); }

int clap_help_requested(cla* cla) { return cla->help_requested; }

int clap_version_requested(cla* cla) { return cla->version_requested; }

void clap_free(cla* cla) {
    ht_free(&cla->args, NULL, ht_free_object);
    vstr_free(&cla->error);
}

void cmd_line_args_free(cla* cla) { ht_free(&cla->args, NULL, ht_free_object); }

static int parse_args(cla* cla, args args, int argc, char* argv[]) {
    int i;
    for (i = 1; i < argc; ++i) {
        char* arg_str = argv[i];
        arg arg;
        int insert_res;
        ssize_t found;
        if ((strncmp(arg_str, "--help", 6) == 0) ||
            (strncmp(arg_str, "-h", 2) == 0)) {
            return HELP_REQUESTED;
        }
        if ((strncmp(arg_str, "--version", 9) == 0) ||
            (strncmp(arg_str, "-v", 2) == 0)) {
            return VERSION_REQUESTED;
        }
        found = vec_find(args, arg_str, &arg, find_arg_fn);
        if (found == -1) {
            cla->error = vstr_format("unkown argument %s", arg_str);
            return CLA_ERROR;
        }
        switch (arg.type) {
        case Null:
            break;
        case Int: {
            const char* name;
            size_t name_len;
            int64_t value;
            object obj;
            i++;
            if (i >= argc) {
                cla->error =
                    vstr_format("%s requires an argument of type int", arg_str);
                return CLA_ERROR;
            }
            value = parse_int_arg(argv[i]);
            obj = object_new(Int, &value);
            name = vstr_data(&arg.name);
            name_len = vstr_len(&arg.name);
            insert_res = ht_try_insert(&cla->args, (void*)name, name_len, &obj);
            if (insert_res == -1) {
                cla->error =
                    vstr_format("failed to insert %s into args table", arg_str);
                return CLA_ERROR;
            }
        } break;
        case String: {
            vstr value;
            object obj;
            const char* name;
            size_t name_len;
            i++;
            if (i >= argc) {
                cla->error = vstr_format(
                    "%s requires an argument of type string", arg_str);
                return CLA_ERROR;
            }
            value = parse_str_arg(argv[i]);
            obj = object_new(String, &value);
            name = vstr_data(&arg.name);
            name_len = vstr_len(&arg.name);
            insert_res = ht_try_insert(&cla->args, (void*)name, name_len, &obj);
            if (insert_res == -1) {
                cla->error =
                    vstr_format("failed to insert %s into args table", arg_str);
                return CLA_ERROR;
            }
        } break;
        }
    }
    return 0;
}

static size_t find_longest_help_line(args args) {
    size_t longest = 0;
    vec_iter iter = vec_iter_new(args);
    while (iter.cur) {
        size_t line_len = 0;
        arg* cur = iter.cur;
        uint32_t flags = cur->flags;
        if (flags & HAS_LONG) {
            size_t long_len = vstr_len(&cur->long_arg);
            line_len += long_len;
        }
        if ((flags & HAS_LONG) && (flags & HAS_SHORT)) {
            line_len += 2;
        } else if (flags & HAS_LONG) {
            line_len += 1;
        }
        if (flags & HAS_SHORT) {
            size_t short_len = vstr_len(&cur->short_arg);
            line_len += short_len;
        }
        line_len += 1;
        if (cur->type != Null) {
            size_t name_len = vstr_len(&cur->name);
            line_len += 3 + name_len;
        }
        longest = max(longest, line_len);
        vec_iter_next(&iter);
    }
    return longest;
}

static void show_help(char* program_name, args args) {
    vec_iter iter = vec_iter_new(args);
    size_t longest = find_longest_help_line(args);
    size_t i, cur_len = 10;

    longest = max(longest, 13);

    printf("usage: %s <options>\n", program_name);

    printf("\t--help, -h");
    for (i = cur_len; i < longest; ++i) {
        printf(" ");
    }
    printf("show this page\n");

    printf("\t--version, -v");
    cur_len = 13;
    for (i = cur_len; i < longest; ++i) {
        printf(" ");
    }
    printf("get the version\n");

    while (iter.cur) {
        arg* arg = iter.cur;
        uint32_t flags = arg->flags;
        cur_len = 0;
        printf("\t");
        if (flags & HAS_LONG) {
            printf("%s", vstr_data(&arg->long_arg));
            cur_len += vstr_len(&arg->long_arg);
        }
        if ((flags & HAS_LONG) && (flags & HAS_SHORT)) {
            printf(", ");
            cur_len += 2;
        } else if (flags & HAS_LONG) {
            printf(" ");
            cur_len += 1;
        }
        if (flags & HAS_SHORT) {
            printf("%s ", vstr_data(&arg->short_arg));
            cur_len += vstr_len(&arg->short_arg) + 1;
        }
        cur_len += 1;
        if (arg->type != Null) {
            printf("<%s> ", vstr_data(&arg->name));
            cur_len += vstr_len(&arg->name) + 3;
        }
        if (flags & HAS_DESCRIPTION) {
            for (i = cur_len; i <= longest; ++i) {
                printf(" ");
            }
            printf("%s\n", vstr_data(&arg->description));
        } else {
            printf("\n");
        }
        vec_iter_next(&iter);
    }
}

static arg arg_new(const char* name, const char* short_arg,
                   const char* long_arg, const char* description,
                   objectt type) {
    vstr vname;
    vstr sarg;
    vstr larg;
    vstr vdesc;
    uint32_t flags = 0;
    arg a = {0};

    assert(name != NULL);
    vname = vstr_from(name);
    a.name = vname;

    if (short_arg) {
        sarg = vstr_from(short_arg);
        flags |= HAS_SHORT;
    } else {
        sarg = vstr_new();
    }
    a.short_arg = sarg;

    if (long_arg) {
        larg = vstr_from(long_arg);
        flags |= HAS_LONG;
    } else {
        larg = vstr_new();
    }
    a.long_arg = larg;

    if (description) {
        vdesc = vstr_from(description);
        flags |= HAS_DESCRIPTION;
    } else {
        vdesc = vstr_new();
    }
    a.description = vdesc;

    a.flags = flags;
    a.type = type;
    return a;
}

static int find_arg_fn(void* arg_str_ptr, void* arg_ptr) {
    char* arg_str = arg_str_ptr;
    arg* arg = arg_ptr;
    size_t arg_str_len = strlen(arg_str);
    vstr long_arg = arg->long_arg;
    vstr short_arg = arg->short_arg;
    size_t long_arg_len = vstr_len(&long_arg);
    size_t short_arg_len = vstr_len(&short_arg);
    if (long_arg_len == arg_str_len) {
        const char* long_arg_str = vstr_data(&long_arg);
        int cmp = strncmp(long_arg_str, arg_str, long_arg_len);
        return cmp;
    } else if (short_arg_len == arg_str_len) {
        const char* short_arg_str = vstr_data(&short_arg);
        int cmp = strncmp(short_arg_str, arg_str, short_arg_len);
        return cmp;
    }
    return 1;
}

static int64_t parse_int_arg(char* str) {
    int64_t val = atoll(str);
    return val;
}

static vstr parse_str_arg(char* str) {
    vstr s = vstr_from(str);
    return s;
}

static void arg_free(void* ptr) {
    arg* arg = ptr;
    vstr_free(&arg->name);
    vstr_free(&arg->short_arg);
    vstr_free(&arg->long_arg);
    vstr_free(&arg->description);
}

static void ht_free_object(void* ptr) {
    object* o = ptr;
    object_free(o);
}
