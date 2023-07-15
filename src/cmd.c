#include "cmd.h"
#include "parser.h"

#include <string.h>
#include <stdio.h>

CmdT cmd_from_bulk(uint8_t* str, size_t str_len) {
    if (str_len != 3) {
        return INV;
    }

    if (memcmp(str, "SET", 3) == 0) {
        return SET;
    }

    if (memcmp(str, "GET", 3) == 0) {
        return GET;
    }

    if (memcmp(str, "DEL", 3) == 0) {
        return DEL;
    }

    return INV;
}

CmdT cmdt_from_statement(Statement* stmt) {
    StatementType type = stmt->type;

    switch (type) {
    case SBULK:
        return cmd_from_bulk(stmt->statement.bulk.str,
                             stmt->statement.bulk.len);
    default:
        return INV;
    }
}

Cmd cmd_from_array(ArrayStatement* astmt) {
    Cmd cmd = { 0 };
    Statement cmd_stmt = astmt->statements[0];
    cmd.type = cmdt_from_statement(&cmd_stmt);

    if (cmd.type == INV) {
        printf("invalid command\n");
        return cmd;
    }

    if (cmd.type == SET) {
        Statement key_stmt;
        Statement val_stmt;
        if (astmt->len != 3) {
            printf("set expects len of 3, got %lu, cmd, key, value\n", astmt->len);
            cmd.type = INV;
            return cmd;
        }
        key_stmt = astmt->statements[1];
        if (key_stmt.type != SBULK) {
            printf("the key must be a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        val_stmt = astmt->statements[2];
        if (val_stmt.type != SBULK) {
            printf("the value must be a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        cmd.expression.set.key.len = key_stmt.statement.bulk.len;
        cmd.expression.set.key.value = key_stmt.statement.bulk.str;
        cmd.expression.set.value.size = val_stmt.statement.bulk.len;
        cmd.expression.set.value.ptr = val_stmt.statement.bulk.str;
        return cmd;
    }

    if (cmd.type == GET) {
        Statement key_stmt;
        if (astmt->len != 2) {
            printf("get expects len of 2, got %lu, cmd, key\n", astmt->len);
            cmd.type = INV;
            return cmd;
        }
        key_stmt = astmt->statements[1];
        if (key_stmt.type != SBULK) {
            printf("the key must be a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        cmd.expression.get.key.len = key_stmt.statement.bulk.len;
        cmd.expression.get.key.value = key_stmt.statement.bulk.str;
        return cmd;
    }

    if (cmd.type == DEL) {
        Statement key_stmt;
        if (astmt->len != 2) {
            printf("del expects len of 2, got %lu, cmd, key\n", astmt->len);
            cmd.type = INV;
            return cmd;
        }
        key_stmt = astmt->statements[1];
        if (key_stmt.type != SBULK) {
            printf("the key must be a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        cmd.expression.del.key.len = key_stmt.statement.bulk.len;
        cmd.expression.del.key.value = key_stmt.statement.bulk.str;
        return cmd;
    }

    cmd.type = INV;
    return cmd;
}

Cmd cmd_from_statement(Statement* stmt) {
    Cmd cmd = { 0 };
    switch (stmt->type) {
    case SARR:
        cmd = cmd_from_array(&(stmt->statement.arr));
        return cmd;
    default:
        cmd.type = INV;
        return cmd;
    }
}

void print_key(Key* key) {
    size_t i, len;
    len = key->len;

    for (i = 0; i < len; ++i) {
        printf("%c", key->value[i]);
    }
}

void print_cmd(Cmd* cmd) {
    if (cmd->type == SET) {
        printf("SET ");
        print_key(&(cmd->expression.set.key));
        printf("->%p(%lu bytes)\n", cmd->expression.set.value.ptr, cmd->expression.set.value.size);
    }
    if (cmd->type == GET) {
        printf("GET ");
        print_key(&(cmd->expression.get.key));
        printf("\n");
    }
    if (cmd->type == DEL) {
        printf("DEL ");
        print_key(&(cmd->expression.del.key));
        printf("\n");
    }
}
