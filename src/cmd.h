#ifndef __CMD_H__

#define __CMD_H__

#include "object.h"
#include "server.h"

typedef enum {
    C_Illegal,
    C_All,
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
    CT_Auth,
    CT_Keys,
} cmd_type;

typedef struct {
    object key;
    object value;
} key_val_cmd;

typedef struct {
    vstr username;
    vstr password;
} auth;

typedef object key_cmd;
typedef object val_cmd;

struct cmd;

typedef int cmd_fn(client* client, const struct cmd* cmd);

typedef struct cmd {
    category cat;
    cmd_type type;
    cmd_fn* proc;
    union {
        key_val_cmd set;
        key_cmd get;
        key_cmd del;
        auth auth;
    } data;
} cmd;

int ping_cmd(client* client, const cmd* cmd);
int set_cmd(client* client, const cmd* cmd);
int get_cmd(client* client, const cmd* cmd);
int del_cmd(client* client, const cmd* cmd);
int auth_cmd(client* client, const cmd* cmd);
int keys_cmd(client* client, const cmd* cmd);
void cmd_free(cmd* cmd);
void cmd_free_full(cmd* cmd);

#endif /* __CMD_H__ */
