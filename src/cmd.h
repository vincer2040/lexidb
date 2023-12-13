#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"
#include <stddef.h>

typedef enum {
    Illegal,
    Ping,
    Set,
    Get,
    Del,
} cmdt;

typedef struct {
    object key;
    object value;
} kv_cmd;

typedef struct {
    object key;
} k_cmd;

typedef kv_cmd set_cmd;
typedef k_cmd get_cmd;
typedef k_cmd del_cmd;

typedef struct {
    cmdt type;
    union {
        set_cmd set;
        get_cmd get;
        del_cmd del;
    } data;
} cmd;

cmd* cmd_new(void);
int cmd_append_arg(cmd** cmd, object* arg);
void cmd_free(cmd* cmd);

#endif /* __CMD_H__ */
