#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"
#include <stddef.h>

struct cmd_help;

typedef struct {
    const char* name;
    size_t types_len;
    const char* type[4];
    int optional;
} cmd_help_argument;

typedef enum {
    Illegal,
    Okc,
    Auth,
    Ping,
    Infoc,
    Select,
    Help,
    Keys,
    Set,
    Get,
    Del,
    Push,
    Pop,
    Enque,
    Deque,
    ZSet,
    ZHas,
    ZDel,
} cmdt;

typedef struct {
    object key;
    object value;
} kv_cmd;

typedef struct {
    object key;
} k_cmd;

typedef struct {
    object value;
} v_cmd;

typedef struct {
    object username;
    object password;
} auth_cmd;

typedef struct {
    int wants_cmd_help;
    cmdt cmd_to_help;
} help_cmd;

typedef kv_cmd set_cmd;
typedef k_cmd get_cmd;
typedef k_cmd del_cmd;
typedef v_cmd push_cmd;
typedef v_cmd enque_cmd;
typedef v_cmd zset_cmd;
typedef v_cmd zhas_cmd;
typedef v_cmd zdel_cmd;
typedef v_cmd select_cmd;

typedef struct {
    cmdt type;
    union {
        auth_cmd auth;
        set_cmd set;
        get_cmd get;
        del_cmd del;
        push_cmd push;
        enque_cmd enque;
        zset_cmd zset;
        zhas_cmd zhas;
        zdel_cmd zdel;
        help_cmd help;
        select_cmd select;
    } data;
} cmd;

#endif /* __CMD_H__ */
