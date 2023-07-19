#include "log.h"
#include "cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void slowlog(uint8_t* buf, size_t len) {
    size_t i;
    LOG(LOG_MSG);
    for (i = 0; i < len; ++i) {
        char at = buf[i];
        printf("%x ", at);
        if (at == ':') {
            size_t k;
            i++;
            for (k = 0; k < 8; ++k, ++i) {
                printf("%x ", buf[i]);
                fflush(stdout);
            }
        }
    }
    printf("\n");
    fflush(stdout);
}

void log_cmd(Cmd* cmd) {
    Key key;
    Value v;
    CmdT t;
    size_t i, key_len;

    LOG(LOG_CMD);

    t = cmd->type;

    if (t == INV) {
        printf("INVALID\n");
        return;
    }

    if (t == CPING) {
        printf("PING\n");
    }

    if (t == SET) {
        key = cmd->expression.set.key;
        v = cmd->expression.set.value;
        printf("SET ");
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }

        printf(" ");

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
    }
    if (t == GET) {
        printf("GET ");
        key = cmd->expression.get.key;
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }
        printf("\n");
        fflush(stdout);
    }
    if (t == DEL) {
        printf("DEL ");
        key = cmd->expression.del.key;
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }
        printf("\n");
        fflush(stdout);
    }
}
