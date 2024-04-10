#include "cmd.h"

void cmd_free(cmd* cmd) {
    switch (cmd->type) {
    case CT_Set:
        object_free(&cmd->data.set.key);
        object_free(&cmd->data.set.value);
        break;
    case CT_Get:
        object_free(&cmd->data.get);
        break;
    case CT_Del:
        object_free(&cmd->data.del);
        break;
    default:
        break;
    }
}
