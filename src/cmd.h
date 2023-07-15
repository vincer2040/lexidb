#ifndef __CMD_H__

#define __CMD_H__

#include "token.h"
#include "parser.h"
#include <stddef.h>

typedef enum { SET, GET, DEL, INV } CmdT;

typedef struct {
    size_t len;
    uint8_t* value;
} Key;

typedef struct {
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

typedef union {
    SetCmd set;
    GetCmd get;
    DelCmd del;
} CmdExpression;

typedef struct {
    CmdT type;
    CmdExpression expression;
} Cmd;

Cmd cmd_from_statement(Statement* stmt);
void print_cmd(Cmd* cmd);

#endif
