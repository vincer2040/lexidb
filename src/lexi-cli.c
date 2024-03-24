#define _XOPEN_SOURCE 600
#include "cmd.h"
#include "hilexi.h"
#include "line_parser.h"
#include "log.h"
#include "object.h"
#include "read_line.h"
#include "vstr.h"
#include <errno.h>
#include <memory.h>
#include <stdio.h>

#define PROMPT "lexi> "

static result(object) execute_cmd(hilexi* l, cmd* cmd);

int main(void) {
    result(hilexi) rl = hilexi_new("127.0.0.1", 6969);
    hilexi l;
    int connect_res, auth_res;

    if (rl.type == Err) {
        error("%s", vstr_data(&(rl.data.err)));
        return 1;
    }

    l = rl.data.ok;
    connect_res = hilexi_connect(&l);
    if (connect_res == -1) {
        error("failed to connect (errno: %d) %s\n", errno, strerror(errno));
        hilexi_close(&l);
        return 1;
    }

    auth_res = hilexi_authenticate(&l, "root", "root");
    if (auth_res != 0) {
        error("failed to authenticate\n");
        hilexi_close(&l);
        return 1;
    }

    for (;;) {
        vstr line;
        const char* line_data;
        size_t line_len;
        cmd cmd;
        result(object) cmd_res;
        object obj;

        printf(PROMPT);
        fflush(stdout);

        line = read_line();
        line_data = vstr_data(&line);
        line_len = vstr_len(&line);

        if (line_len == 0) {
            vstr_free(&line);
            continue;
        }

        if (line_len == 4 && memcmp(line_data, "exit", 4) == 0) {
            vstr_free(&line);
            break;
        }

        cmd = parse_line(line_data, line_len);

        cmd_res = execute_cmd(&l, &cmd);

        vstr_free(&line);
        if (cmd_res.type == Err) {
            error("%s\n", vstr_data(&(cmd_res.data.err)));
            vstr_free(&(cmd_res.data.err));
            continue;
        }

        obj = cmd_res.data.ok;
        object_show(&obj);
        object_free(&obj);
    }
    hilexi_close(&l);
    return 0;
}

static result(object) execute_cmd(hilexi* l, cmd* cmd) {
    result(object) cmd_res = {0};
    switch (cmd->type) {
    case Ping:
        cmd_res = hilexi_ping(l);
        break;
    case Infoc:
        cmd_res = hilexi_info(l);
        break;
    case Help: {
        vstr h = vstr_from("help");
        cmd_res.type = Ok;
        cmd_res.data.ok = object_new(String, &h);
    } break;
    case Select:
        cmd_res = hilexi_select(l, cmd->data.select.value.data.num);
        break;
    case Keys:
        cmd_res = hilexi_keys(l);
        break;
    case Set: {
        set_cmd set = cmd->data.set;
        object key = set.key;
        object value = set.value;
        cmd_res = hilexi_set(l, &key, &value);
        object_free(&key);
        object_free(&value);
    } break;
    case Get: {
        get_cmd get = cmd->data.get;
        object key = get.key;
        cmd_res = hilexi_get(l, &key);
        object_free(&key);
    } break;
    case Del: {
        del_cmd del = cmd->data.del;
        object key = del.key;
        cmd_res = hilexi_del(l, &key);
        object_free(&key);
    } break;
    case Push: {
        push_cmd push = cmd->data.push;
        object value = push.value;
        cmd_res = hilexi_push(l, &value);
        object_free(&value);
    } break;
    case Pop:
        cmd_res = hilexi_pop(l);
        break;
    case Enque: {
        enque_cmd enque = cmd->data.enque;
        object value = enque.value;
        cmd_res = hilexi_enque(l, &value);
        object_free(&value);
    } break;
    case Deque:
        cmd_res = hilexi_deque(l);
        break;
    case ZSet: {
        object value = cmd->data.zset.value;
        cmd_res = hilexi_zset(l, &value);
        object_free(&value);
    } break;
    case ZHas: {
        object value = cmd->data.zhas.value;
        cmd_res = hilexi_zhas(l, &value);
        object_free(&value);
    } break;
    case ZDel: {
        object value = cmd->data.zdel.value;
        cmd_res = hilexi_zdel(l, &value);
        object_free(&value);
    } break;
    default:
        cmd_res.type = Err;
        cmd_res.data.err = vstr_from("invalid command");
        break;
    }
    return cmd_res;
}
