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

#define HELP_TEXT "\
commands:\n\
    help                                = show this screen\n\
    set <key> <value>                   = set a key and value\n\
    get <key>                           = get a value\n\
    del <key>                           = delete a value\n\
    push <value>                        = push a value\n\
    pop                                 = pop value\n\
\n\
    clear                               = clear the screen\n\
    exit                                = exit the process\n\
"

void evaluate_cmd(HiLexi* l, CliCmd* cmd) {
    if (cmd->type == CC_SET) {
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
        return;
    }
    if (cmd->type == CC_GET) {
        uint8_t* key = cmd->expr.get.key.value;
        size_t key_len = cmd->expr.get.key.len;
        hilexi_get(l, key, key_len);
        return;
    }
    if (cmd->type == CC_DEL) {
        uint8_t* key = cmd->expr.del.key.value;
        size_t key_len = cmd->expr.del.key.len;
        hilexi_del(l, key, key_len);
        return;
    }
    if (cmd->type == CC_PUSH) {
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
        return;
    }
    if (cmd->type == CC_POP) {
        hilexi_pop(l);
        return;
    }
    if (cmd->type == CC_PING) {
        hilexi_ping(l);
        return;
    }
    if (cmd->type == CC_KEYS) {
        hilexi_keys(l);
        return;
    }
    if (cmd->type == CC_HELP) {
        printf(HELP_TEXT);
        return;
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
