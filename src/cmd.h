#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"

typedef enum {
    CT_Illegal,
    CT_Info,
    CT_Set,
    CT_Get,
    CT_Del,
} cmd_type;

typedef struct {
    object key;
    object value;
} key_val_cmd;

typedef object key_cmd;
typedef object val_cmd;

typedef struct {
    cmd_type type;
    union {
        key_val_cmd set;
        key_cmd get;
        key_cmd del;
    } data;
} cmd;

void cmd_free(cmd* cmd);

#endif /* __CMD_H__ */
