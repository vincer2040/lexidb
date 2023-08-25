#ifndef __CLI_CMD_H__

#define __CLI_CMD_H__

#include "cmd.h"

typedef enum {
    CC_INV,
    CC_HELP,
    CC_PING,
    CC_SET,
    CC_GET,
    CC_DEL,
    CC_KEYS,
    CC_VALUES,
    CC_ENTRIES,
    CC_PUSH,
    CC_POP,
    CC_CLUSTER_NEW,
    CC_CLUSTER_DROP,
    CC_CLUSTER_SET,
    CC_CLUSTER_GET,
    CC_CLUSTER_DEL,
    CC_CLUSTER_PUSH,
    CC_CLUSTER_POP,
} CliCmdT;

typedef struct {
    CliCmdT type;
    CmdExpression expr;
} CliCmd;

CliCmd cli_parse_cmd(char* input, size_t input_len);
void cli_cmd_print(CliCmd* cmd);

#endif
