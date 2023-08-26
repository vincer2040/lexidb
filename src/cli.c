#include "cli-cmd.h"
#include "cli-util.h"
#include "hilexi.h"
#include "sock.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ADDR "127.0.0.1"
#define PORT 6969

#define HELP_TEXT                                                              \
    "\
commands:\n\
    help                                = show this screen\n\
    set <key> <value>                   = set a key and value\n\
    get <key>                           = get a value\n\
    del <key>                           = delete a value\n\
    keys                                = get all the keys\n\
    values                              = get all the values\n\
    entries                             = get all the entries\n\
    push <value>                        = push a value\n\
    pop                                 = pop value\n\
\n\
    cluster.new <name> <key>            = create a new cluster\n\
    cluster.drop <name>                 = delete an entire cluster\n\
    cluster.set <name> <key> <value>    = set a key and value in a cluster\n\
    cluster.get <name> <key>            = get a value from a cluster\n\
    cluster.del <name> <key>            = delete a value from a cluster\n\
    cluster.push <name> <value>         = push a value in a cluster\n\
    cluster.pop <name>                  = pop a value from a cluster\n\
\n\
    clear                               = clear the screen\n\
    exit                                = exit the process\n\
"

void evaluate_cmd(HiLexi* l, CliCmd* cmd) {
    switch (cmd->type) {
    case CC_SET: {
        uint8_t* key = cmd->expr.set.key.value;
        size_t key_len = cmd->expr.set.key.len;
        ValueT vt = cmd->expr.set.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.set.value.ptr));
            if (hilexi_set_int(l, key, key_len, val) != 0) {
                printf("set int fail\n");
                return;
            }
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.set.value.ptr;
            size_t val_size = cmd->expr.set.value.size;
            if (hilexi_set(l, key, key_len, val, val_size) != 0) {
                printf("set string fail\n");
                return;
            }
        } else {
            printf("invalid value\n");
            return;
        }
    } break;
    case CC_GET: {
        uint8_t* key = cmd->expr.get.key.value;
        size_t key_len = cmd->expr.get.key.len;
        hilexi_get(l, key, key_len);
        return;
    }
    case CC_DEL: {
        uint8_t* key = cmd->expr.del.key.value;
        size_t key_len = cmd->expr.del.key.len;
        hilexi_del(l, key, key_len);
    } break;
    case CC_PUSH: {
        ValueT vt = cmd->expr.push.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.push.value.ptr));
            if (hilexi_push_int(l, val) != 0) {
                printf("push int fail\n");
                return;
            }
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.push.value.ptr;
            size_t val_size = cmd->expr.push.value.size;
            if (hilexi_push(l, val, val_size) != 0) {
                printf("push string fail\n");
                return;
            }
        } else {
            printf("invalid value");
            return;
        }
    } break;
    case CC_POP:
        hilexi_pop(l);
        break;
    case CC_PING:
        hilexi_ping(l);
        break;
    case CC_KEYS:
        hilexi_keys(l);
        break;
    case CC_VALUES:
        hilexi_values(l);
        break;
    case CC_ENTRIES:
        hilexi_entries(l);
        break;
    case CC_HELP:
        printf(HELP_TEXT);
        break;
    case CC_CLUSTER_NEW: {
        uint8_t* name = cmd->expr.cluster_new.cluster_name.value;
        size_t name_len = cmd->expr.cluster_new.cluster_name.len;
        hilexi_cluster_new(l, name, name_len);
    } break;
    case CC_CLUSTER_DROP: {
        uint8_t* name = cmd->expr.cluster_drop.cluster_name.value;
        size_t name_len = cmd->expr.cluster_drop.cluster_name.len;
        hilexi_cluster_drop(l, name, name_len);
    } break;
    case CC_CLUSTER_SET: {
        uint8_t* name = cmd->expr.cluster_set.cluster_name.value;
        size_t name_len = cmd->expr.cluster_set.cluster_name.len;
        uint8_t* key = cmd->expr.cluster_set.set.key.value;
        size_t key_len = cmd->expr.cluster_set.set.key.len;
        ValueT vt = cmd->expr.cluster_set.set.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.cluster_set.set.value.ptr));
            if (hilexi_cluster_set_int(l, name, name_len, key, key_len, val) != 0) {
                printf("set int fail\n");
                return;
            }
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.cluster_set.set.value.ptr;
            size_t val_size = cmd->expr.cluster_set.set.value.size;
            if (hilexi_cluster_set(l, name, name_len, key, key_len, val, val_size) != 0) {
                printf("set string fail\n");
                return;
            }
        } else {
            printf("invalid value\n");
            return;
        }
    } break;
    case CC_CLUSTER_GET: {
        uint8_t* name = cmd->expr.cluster_get.cluster_name.value;
        size_t name_len = cmd->expr.cluster_get.cluster_name.len;
        uint8_t* key = cmd->expr.cluster_get.get.key.value;
        size_t key_len = cmd->expr.cluster_get.get.key.len;
        hilexi_cluster_get(l, name, name_len, key, key_len);
    } break;
    case CC_CLUSTER_DEL: {
        uint8_t* name = cmd->expr.cluster_del.cluster_name.value;
        size_t name_len = cmd->expr.cluster_del.cluster_name.len;
        uint8_t* key = cmd->expr.cluster_del.del.key.value;
        size_t key_len = cmd->expr.cluster_del.del.key.len;
        hilexi_cluster_del(l, name, name_len, key, key_len);
    } break;
    case CC_CLUSTER_PUSH: {
        uint8_t* name = cmd->expr.cluster_push.cluster_name.value;
        size_t name_len = cmd->expr.cluster_push.cluster_name.len;
        ValueT vt = cmd->expr.cluster_push.push.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.cluster_push.push.value.ptr));
            if (hilexi_cluster_push_int(l, name, name_len, val) != 0) {
                printf("set int fail\n");
                return;
            }
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.cluster_push.push.value.ptr;
            size_t val_size = cmd->expr.cluster_push.push.value.size;
            if (hilexi_cluster_push(l, name, name_len, val, val_size) != 0) {
                printf("set string fail\n");
                return;
            }
        } else {
            printf("invalid value\n");
            return;
        }
    } break;
    case CC_CLUSTER_POP: {
        uint8_t* name = cmd->expr.cluster_pop.cluster_name.value;
        size_t name_len = cmd->expr.cluster_pop.cluster_name.len;
        hilexi_cluster_pop(l, name, name_len);
    } break;
    default:
        printf("invalid command\n");
        break;
    }
}

void clear() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    system("clear");
#endif

#if defined(_WIN32) || defined(_WIN64)
    system("cls");
#endif
}

int run() {
    HiLexi* l;

    l = hilexi_new(ADDR, PORT);

    if (hilexi_connect(l) == -1) {
        printf("failed to connect\n");
        hilexi_destory(l);
        return -1;
    }

    for (;;) {
        char* line = get_line("lexi>");
        CliCmd cmd;

        if (line == NULL) {
            goto err;
        }

        // user just pressed enter
        if (*line == '\0') {
            free(line);
            continue;
        }

        // clear the console screen
        if (strncmp(line, "clear", 4) == 0) {
            free(line);
            clear();
            continue;
        }

        // exit the process
        if (strncmp(line, "exit", 4) == 0) {
            printf("goodbye\n");
            free(line);
            break;
        }

        cmd = cli_parse_cmd(line, strlen(line));
        evaluate_cmd(l, &cmd);
        free(line);
    }

    hilexi_destory(l);
    return 0;

err:
    hilexi_destory(l);
    return -1;
}

int main() {
    run();
    return 0;
}
