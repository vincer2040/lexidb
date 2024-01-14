#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"
#include <stddef.h>

typedef enum {
    Illegal,
    Okc,
    Auth,
    Ping,
    Infoc,
    Help,
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

typedef kv_cmd set_cmd;
typedef k_cmd get_cmd;
typedef k_cmd del_cmd;
typedef v_cmd push_cmd;
typedef v_cmd enque_cmd;
typedef v_cmd zset_cmd;
typedef v_cmd zhas_cmd;
typedef v_cmd zdel_cmd;

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
    } data;
} cmd;

#endif /* __CMD_H__ */
