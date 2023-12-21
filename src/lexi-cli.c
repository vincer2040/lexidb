#define _XOPEN_SOURCE 600
#include "cmd.h"
#include "hilexi.h"
#include "line_parser.h"
#include "log.h"
#include "object.h"
#include "read_line.h"
#include "vstr.h"
#include <memory.h>
#include <stdio.h>
#include <time.h>

#define PROMPT "> "

// static void print_cmd(cmd* cmd);

int main(void) {
    result(hilexi) rl = hilexi_new("127.0.0.1", 6969);
    hilexi l;
    object obj;
    if (rl.type == Err) {
        error("%s\n", vstr_data(&(rl.data.err)));
        vstr_free(&(rl.data.err));
        return 1;
    }
    l = rl.data.ok;
    hilexi_connect(&l);
    obj = hilexi_ping(&l);
    object_show(&obj);
    object_free(&obj);
    /*
    for (;;) {
        vstr line;
        const char* line_data;
        size_t line_len;
        cmd cmd;

        printf(PROMPT);
        fflush(stdout);

        line = read_line();
        line_data = vstr_data(&line);
        line_len = vstr_len(&line);

        if (line_len == 4 && memcmp(line_data, "exit", 4) == 0) {
            vstr_free(&line);
            break;
        }

        cmd = parse_line(line_data, line_len);
        print_cmd(&cmd);

        printf("%s\n", line_data);
        vstr_free(&line);
    }
    */
    hilexi_close(&l);
    return 0;
}

/*
static void print_cmd(cmd* cmd) {
    switch (cmd->type) {
    case Okc:
        break;
    case Illegal:
        printf("illegal\n");
        break;
    case Ping:
        printf("ping\n");
        break;
    case Set:
        printf("set\n");
        object_show(&cmd->data.set.key);
        object_show(&cmd->data.set.value);
        break;
    case Get:
        printf("get\n");
        object_show(&cmd->data.get.key);
        break;
    case Del:
        printf("del\n");
        object_show(&cmd->data.del.key);
        break;
        break;
    }
}
*/
