#ifndef __CMD_H__

#define __CMD_H__

#include "token.h"
#include "parser.h"
#include <stddef.h>

struct Cmd;

typedef enum {
    INV,
    COK,
    CPING,
    SET,
    GET,
    DEL,
    KEYS,
    VALUES,
    ENTRIES,
    PUSH,
    POP,
    ENQUE,
    DEQUE,
    CLUSTER_NEW,
    CLUSTER_DROP,
    CLUSTER_SET,
    CLUSTER_GET,
    CLUSTER_DEL,
    CLUSTER_PUSH,
    CLUSTER_POP,
    CLUSTER_KEYS,
    CLUSTER_VALUES,
    CLUSTER_ENTRIES,
    MULTI_CMD,
    REPLICATE,
    REPLICATION,
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
    Value value;
} EnqueCmd;

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

typedef struct {
    Key cluster_name;
} ClusterKeysCmd;

typedef struct {
    Key cluster_name;
} ClusterValuesCmd;

typedef struct {
    Key cluster_name;
} ClusterEntriesCmd;

typedef struct {
    size_t len;
    struct Cmd* commands;
} MultiCmd;

typedef union {
    SetCmd set;
    GetCmd get;
    DelCmd del;
    PushCmd push;
    EnqueCmd enque;
    ClusterNewCmd cluster_new;
    ClusterDropCmd cluster_drop;
    ClusterSetCmd cluster_set;
    ClusterGetCmd cluster_get;
    ClusterDelCmd cluster_del;
    ClusterPushCmd cluster_push;
    ClusterPopCmd cluster_pop;
    ClusterKeysCmd cluster_keys;
    ClusterValuesCmd cluster_values;
    ClusterEntriesCmd cluster_entries;
    MultiCmd multi;
} CmdExpression;

typedef struct Cmd {
    CmdT type;
    CmdExpression expression;
} Cmd;

Cmd cmd_from_statement(Statement* stmt);
void cmd_print(Cmd* cmd);

#endif
