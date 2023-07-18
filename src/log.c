#include "cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void slowlog(FILE* stream, uint8_t* buf, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        fputc(buf[i], stream);
    }
}

void log_cmd(FILE* stream, Cmd* cmd) {
    Key key;
    CmdT t;
    size_t i, key_len;

    t = cmd->type;

    if (t == INV) {
        fputs("INVALID\n", stream);
        return;
    }

    key = cmd->expression.set.key;
    key_len = key.len;

    if (t == SET) {
        fputs("SET ", stream);
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }
        fputc('\n', stream);
    }
    if (t == GET) {
        fputs("GET ", stream);
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }
        fputc('\n', stream);
    }
    if (t == DEL) {
        fputs("DEL ", stream);
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            fputc(key.value[i], stream);
        }
        fputc('\n', stream);
    }
}
