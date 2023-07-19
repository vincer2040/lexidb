#include "cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void slowlog(FILE* stream, uint8_t* buf, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        char at = buf[i];
        fputc(at, stream);
        if (at == ':') {
            size_t k;
            i++;
            for (k = 0; k < 8; ++k, ++i) {
                printf("%x", buf[i]);
                fflush(stdout);
            }
        }
    }
}

void log_cmd(FILE* stream, Cmd* cmd) {
    Key key;
    Value v;
    CmdT t;
    size_t i, key_len;

    t = cmd->type;

    if (t == INV) {
        fputs("INVALID\n", stream);
        return;
    }


    if (t == SET) {
        key = cmd->expression.set.key;
        v = cmd->expression.set.value;
        fputs("SET ", stream);
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }

        fputs(" ", stream);

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                fputc(val[i], stream);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        fputc('\n', stream);
    }
    if (t == GET) {
        key = cmd->expression.get.key;
        fputs("GET ", stream);
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }
        fputc('\n', stream);
    }
    if (t == DEL) {
        key = cmd->expression.del.key;
        fputs("DEL ", stream);
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }
        fputc('\n', stream);
    }
}
