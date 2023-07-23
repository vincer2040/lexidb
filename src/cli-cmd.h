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
}CliCmdT;

typedef struct {
    CliCmdT type;
    CmdExpression expr;
}CliCmd;

CliCmd cli_parse_cmd(char* input, size_t input_len);
void cli_cmd_print(CliCmd* cmd);

#endif
