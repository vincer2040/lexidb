#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"
#include "server.h"

typedef enum {
    C_Illegal,
    C_Admin,
    C_Read,
    C_Write,
    C_Connection,
} category;

typedef enum {
    CT_Illegal,
    CT_Ping,
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

struct cmd;

typedef void cmd_fn(client* client, const struct cmd* cmd);

typedef struct cmd {
    category cat;
    cmd_type type;
    cmd_fn* proc;
    union {
        key_val_cmd set;
        key_cmd get;
        key_cmd del;
    } data;
} cmd;

void ping_cmd_fn(client* client, const cmd* cmd);
void set_cmd_fn(client* client, const cmd* cmd);
void get_cmd_fn(client* client, const cmd* cmd);
void del_cmd_fn(client* client, const cmd* cmd);
void cmd_free(cmd* cmd);

#endif /* __CMD_H__ */
