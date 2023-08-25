#ifndef __CMD_H__

#define __CMD_H__

#include "token.h"
#include "parser.h"
#include <stddef.h>

typedef enum {
    INV,
    CPING,
    SET,
    GET,
    DEL,
    KEYS,
    VALUES,
    ENTRIES,
    PUSH,
    POP,
    CLUSTER_NEW,
    CLUSTER_DROP,
    CLUSTER_SET,
    CLUSTER_GET,
    CLUSTER_DEL,
    CLUSTER_PUSH,
    CLUSTER_POP,
} CmdT;

typedef struct {
    size_t len;
    uint8_t* value;
} Key;

typedef enum {
    VTSTRING,
    VTINT,
    VTNULL
}ValueT;

typedef struct {
    ValueT type;
    size_t size;
    void* ptr;
} Value;

typedef struct {
    Key key;
    Value value;
} SetCmd;

typedef struct {
    Key key;
} GetCmd;

typedef struct {
    Key key;
} DelCmd;

typedef struct {
    Value value;
} PushCmd;

typedef struct {
    Key cluster_name;
} ClusterNewCmd;

typedef struct {
    Key cluster_name;
} ClusterDropCmd;

typedef struct {
    Key cluster_name;
    SetCmd set;
} ClusterSetCmd;

typedef struct {
    Key cluster_name;
    GetCmd get;
} ClusterGetCmd;

typedef struct {
    Key cluster_name;
    DelCmd del;
} ClusterDelCmd;

typedef struct {
    Key cluster_name;
    PushCmd push;
} ClusterPushCmd;

typedef struct {
    Key cluster_name;
} ClusterPopCmd;

typedef union {
    SetCmd set;
    GetCmd get;
    DelCmd del;
    PushCmd push;
    ClusterNewCmd cluster_new;
    ClusterDropCmd cluster_drop;
    ClusterSetCmd cluster_set;
    ClusterGetCmd cluster_get;
    ClusterDelCmd cluster_del;
    ClusterPushCmd cluster_push;
    ClusterPopCmd cluster_pop;
} CmdExpression;

typedef struct {
    CmdT type;
    CmdExpression expression;
} Cmd;

Cmd cmd_from_statement(Statement* stmt);
void cmd_print(Cmd* cmd);

#endif
