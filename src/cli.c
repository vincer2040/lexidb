#include "cli-cmd.h"
#include "cli-util.h"
#include "config.h"
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
    enque <value>                       = enque value\n\
    deque                               = enque value\n\
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

static void data_print(HiLexiData* data);

void evaluate_cmd(HiLexi* l, CliCmd* cmd) {
    HiLexiData data = {0};
    switch (cmd->type) {
    case CC_SET: {
        uint8_t* key = cmd->expr.set.key.value;
        size_t key_len = cmd->expr.set.key.len;
        ValueT vt = cmd->expr.set.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.set.value.ptr));
            data = hilexi_set_int(l, (const char*)key, key_len, val);
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.set.value.ptr;
            size_t val_size = cmd->expr.set.value.size;
            data = hilexi_set(l, (const char*)key, key_len, val, val_size);
        } else {
            printf("invalid value\n");
            return;
        }
    } break;
    case CC_GET: {
        uint8_t* key = cmd->expr.get.key.value;
        size_t key_len = cmd->expr.get.key.len;
        data = hilexi_get(l, (const char*)key, key_len);
        break;
    }
    case CC_DEL: {
        uint8_t* key = cmd->expr.del.key.value;
        size_t key_len = cmd->expr.del.key.len;
        data = hilexi_del(l, (const char*)key, key_len);
    } break;
    case CC_PUSH: {
        ValueT vt = cmd->expr.push.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.push.value.ptr));
            data = hilexi_push_int(l, val);
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.push.value.ptr;
            size_t val_size = cmd->expr.push.value.size;
            data = hilexi_push(l, val, val_size);
        } else {
            printf("invalid value");
            return;
        }
    } break;
    case CC_POP:
        data = hilexi_pop(l);
        break;
    case CC_ENQUE: {
        ValueT vt = cmd->expr.push.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.enque.value.ptr));
            data = hilexi_enque_int(l, val);
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.enque.value.ptr;
            size_t val_size = cmd->expr.enque.value.size;
            data = hilexi_enque(l, val, val_size);
        } else {
            printf("invalid value");
            return;
        }
    } break;
    case CC_DEQUE:
        data = hilexi_deque(l);
        break;
    case CC_PING:
        data = hilexi_ping(l);
        break;
    case CC_KEYS:
        data = hilexi_keys(l);
        break;
    case CC_VALUES:
        data = hilexi_values(l);
        break;
    case CC_ENTRIES:
        data = hilexi_entries(l);
        break;
    case CC_HELP:
        printf(HELP_TEXT);
        return;
    case CC_CLUSTER_NEW: {
        uint8_t* name = cmd->expr.cluster_new.cluster_name.value;
        size_t name_len = cmd->expr.cluster_new.cluster_name.len;
        data = hilexi_cluster_new(l, (const char*)name, name_len);
    } break;
    case CC_CLUSTER_DROP: {
        uint8_t* name = cmd->expr.cluster_drop.cluster_name.value;
        size_t name_len = cmd->expr.cluster_drop.cluster_name.len;
        data = hilexi_cluster_drop(l, (const char*)name, name_len);
    } break;
    case CC_CLUSTER_SET: {
        uint8_t* name = cmd->expr.cluster_set.cluster_name.value;
        size_t name_len = cmd->expr.cluster_set.cluster_name.len;
        uint8_t* key = cmd->expr.cluster_set.set.key.value;
        size_t key_len = cmd->expr.cluster_set.set.key.len;
        ValueT vt = cmd->expr.cluster_set.set.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.cluster_set.set.value.ptr));
            data = hilexi_cluster_set_int(l, (const char*)name, name_len,
                                          (const char*)key, key_len, val);
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.cluster_set.set.value.ptr;
            size_t val_size = cmd->expr.cluster_set.set.value.size;
            data = hilexi_cluster_set(l, (const char*)name, name_len,
                                      (const char*)key, key_len, val, val_size);
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
        data = hilexi_cluster_get(l, (const char*)name, name_len,
                                  (const char*)key, key_len);
    } break;
    case CC_CLUSTER_DEL: {
        uint8_t* name = cmd->expr.cluster_del.cluster_name.value;
        size_t name_len = cmd->expr.cluster_del.cluster_name.len;
        uint8_t* key = cmd->expr.cluster_del.del.key.value;
        size_t key_len = cmd->expr.cluster_del.del.key.len;
        data = hilexi_cluster_del(l, (const char*)name, name_len,
                                  (const char*)key, key_len);
    } break;
    case CC_CLUSTER_PUSH: {
        uint8_t* name = cmd->expr.cluster_push.cluster_name.value;
        size_t name_len = cmd->expr.cluster_push.cluster_name.len;
        ValueT vt = cmd->expr.cluster_push.push.value.type;
        if (vt == VTINT) {
            int64_t val = ((int64_t)(cmd->expr.cluster_push.push.value.ptr));
            data = hilexi_cluster_push_int(l, (const char*)name, name_len, val);
        } else if (vt == VTSTRING) {
            char* val = cmd->expr.cluster_push.push.value.ptr;
            size_t val_size = cmd->expr.cluster_push.push.value.size;
            data = hilexi_cluster_push(l, (const char*)name, name_len, val,
                                       val_size);
        } else {
            printf("invalid value\n");
            return;
        }
    } break;
    case CC_CLUSTER_POP: {
        uint8_t* name = cmd->expr.cluster_pop.cluster_name.value;
        size_t name_len = cmd->expr.cluster_pop.cluster_name.len;
        data = hilexi_cluster_pop(l, (const char*)name, name_len);
    } break;
    case CC_CLUSTER_KEYS: {
        uint8_t* name = cmd->expr.cluster_keys.cluster_name.value;
        size_t name_len = cmd->expr.cluster_keys.cluster_name.len;
        data = hilexi_cluster_keys(l, (const char*)name, name_len);
    } break;
    case CC_CLUSTER_VALUES: {
        uint8_t* name = cmd->expr.cluster_values.cluster_name.value;
        size_t name_len = cmd->expr.cluster_values.cluster_name.len;
        data = hilexi_cluster_values(l, (const char*)name, name_len);
    } break;
    case CC_CLUSTER_ENTRIES: {
        uint8_t* name = cmd->expr.cluster_entries.cluster_name.value;
        size_t name_len = cmd->expr.cluster_entries.cluster_name.len;
        data = hilexi_cluster_entries(l, (const char*)name, name_len);
    } break;
    case CC_STATS_CYCLES:
        data = hilexi_stats_cycles(l);
        break;
    default:
        printf("invalid command\n");
        return;
    }

    data_print(&data);
    hilexi_data_free(&data);
}

static void data_print(HiLexiData* data) {
    switch (data->type) {
    case HL_ERR:
        printf("error\n");
        break;
    case HL_ARR:{
        VecIter iter = vec_iter_new(data->val.arr, 0);
        size_t i, len = data->val.arr->len;
        for (i = 0; i < len; ++i) {
            HiLexiData* cur = iter.cur;
            data_print(cur);
            vec_iter_next(&iter);
        }
    } break;
    case HL_BULK_STRING:
        printf("\"%s\"\n", data->val.string);
        break;
    case HL_SIMPLE_STRING:
        switch (data->val.simple) {
        case HL_INVSS:
            printf("invalid simple\n");
            break;
        case HL_PONG:
            printf("PONG\n");
            break;
        case HL_NONE:
            printf("NONE\n");
            break;
        case HL_OK:
            printf("OK\n");
            break;
        }
        break;
    case HL_INT:
        printf("(int) %ld\n", data->val.integer);
        break;
    case HL_ERR_STR:
        printf("%s\n", data->val.string);
        break;
    default:
        printf("invalid data\n");
        break;
    }
}

void clear() {
#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
    int r = system("clear");
    ((void)r);
#endif

#if defined(_WIN32) || defined(_WIN64)
    int r = system("cls");
    ((void)r);
#endif
}

int run(const char* addr, uint16_t port) {
    HiLexi* l;

    l = hilexi_new(addr, port);

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

int main(int argc, char** argv) {
    int default_port = PORT;
    Configuration* config;
    Ht* args;

    config = config_new();

    config_add_option(&config, "--port", "-p", COT_INT, &default_port,
                      "the port to connect to");

    args = configure(config, argc, argv);

    if (args) {
        Object* port = ht_get(args, (uint8_t*)"--port", 6);
        config_free(config);

        run("127.0.0.1", (uint16_t)port->data.i64);
        free_configuration_ht(args);
        return 0;
    } else {
        config_free(config);
        return 0;
    }
}
