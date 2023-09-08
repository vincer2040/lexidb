#include "cmd.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>

CmdT cmd_from_bulk(uint8_t* str, size_t str_len) {
    if (str_len == 3) {

        if (memcmp(str, "SET", 3) == 0) {
            return SET;
        }

        if (memcmp(str, "GET", 3) == 0) {
            return GET;
        }

        if (memcmp(str, "DEL", 3) == 0) {
            return DEL;
        }

        if (memcmp(str, "POP", 3) == 0) {
            return POP;
        }

        return INV;
    }

    if (str_len == 4) {

        if (memcmp(str, "PUSH", 4) == 0) {
            return PUSH;
        }

        if (memcmp(str, "KEYS", 4) == 0) {
            return KEYS;
        }

        return INV;
    }

    if (str_len == 6) {
        if (memcmp(str, "VALUES", 6) == 0) {
            return VALUES;
        }

        return INV;
    }

    if (str_len == 7) {
        if (memcmp(str, "ENTRIES", 7) == 0) {
            return ENTRIES;
        }

        return INV;
    }

    if (str_len == 11) {
        if (memcmp(str, "CLUSTER.NEW", 11) == 0) {
            return CLUSTER_NEW;
        }

        if (memcmp(str, "CLUSTER.SET", 11) == 0) {
            return CLUSTER_SET;
        }

        if (memcmp(str, "CLUSTER.GET", 11) == 0) {
            return CLUSTER_GET;
        }

        if (memcmp(str, "CLUSTER.DEL", 11) == 0) {
            return CLUSTER_DEL;
        }

        if (memcmp(str, "CLUSTER.POP", 11) == 0) {
            return CLUSTER_POP;
        }

        return INV;
    }

    if (str_len == 12) {
        if (memcmp(str, "CLUSTER.DROP", 12) == 0) {
            return CLUSTER_DROP;
        }

        if (memcmp(str, "CLUSTER.PUSH", 12) == 0) {
            return CLUSTER_PUSH;
        }

        if (memcmp(str, "CLUSTER.KEYS", 12) == 0) {
            return CLUSTER_KEYS;
        }

        return INV;
    }

    if (str_len == 14) {
        if (memcmp(str, "CLUSTER.VALUES", 14) == 0) {
            return CLUSTER_VALUES;
        }

        return INV;
    }

    if (str_len == 15) {
        if (memcmp(str, "CLUSTER.ENTRIES", 15) == 0) {
            return CLUSTER_ENTRIES;
        }

        return INV;
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
    Cmd cmd = {0};
    Statement cmd_stmt = astmt->statements[0];
    cmd.type = cmdt_from_statement(&cmd_stmt);

    switch (cmd.type) {
    case INV: {
        printf("invalid command\n");
        return cmd;
    }

    case SET: {
        Statement key_stmt;
        Statement val_stmt;
        if (astmt->len != 3) {
            printf("set expects len of 3, got %lu, cmd, key, value\n",
                   astmt->len);
            cmd.type = INV;
            return cmd;
        }
        key_stmt = astmt->statements[1];
        if (key_stmt.type != SBULK) {
            printf("the key must be a bulk string\n");
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.set.key.len = key_stmt.statement.bulk.len;
        cmd.expression.set.key.value = key_stmt.statement.bulk.str;

        val_stmt = astmt->statements[2];
        if (val_stmt.type == SBULK) {
            cmd.expression.set.value.type = VTSTRING;
            cmd.expression.set.value.size = val_stmt.statement.bulk.len;
            cmd.expression.set.value.ptr = val_stmt.statement.bulk.str;
            return cmd;
        } else if (val_stmt.type == SINT) {
            cmd.expression.set.value.type = VTINT;
            cmd.expression.set.value.size = 8;
            cmd.expression.set.value.ptr = ((void*)(val_stmt.statement.i64));
            return cmd;
        } else {
            cmd.type = INV;
        }
        return cmd;
    }

    case GET: {
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

    case DEL: {
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

    case PUSH: {
        Statement val_stmt;
        if (astmt->len != 2) {
            printf("push expects len of 2, got %lu, cmd, value\n", astmt->len);
            cmd.type = INV;
            return cmd;
        }

        val_stmt = astmt->statements[1];
        if (val_stmt.type == SBULK) {
            cmd.expression.push.value.type = VTSTRING;
            cmd.expression.push.value.size = val_stmt.statement.bulk.len;
            cmd.expression.push.value.ptr = val_stmt.statement.bulk.str;
            return cmd;
        } else if (val_stmt.type == SINT) {
            cmd.expression.push.value.type = VTINT;
            cmd.expression.push.value.size = 8;
            cmd.expression.push.value.ptr = ((void*)(val_stmt.statement.i64));
            return cmd;
        } else {
            cmd.type = INV;
        }
        return cmd;
    }

    case CLUSTER_NEW: {
        Statement name_stmt;
        if (astmt->len != 2) {
            printf("cluster new expects len of 2, got %lu, cmd, name\n",
                   astmt->len);
            cmd.type = INV;
            return cmd;
        }
        name_stmt = astmt->statements[1];
        if (name_stmt.type != SBULK) {
            printf("cluster key not a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        cmd.expression.cluster_new.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_new.cluster_name.len =
            name_stmt.statement.bulk.len;
        return cmd;
    }

    case CLUSTER_DROP: {
        Statement name_stmt;
        if (astmt->len != 2) {
            printf("cluster drop expects len of 2, got %lu, cmd, name\n",
                   astmt->len);
            cmd.type = INV;
            return cmd;
        }
        name_stmt = astmt->statements[1];
        if (name_stmt.type != SBULK) {
            printf("cluster key not a bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        cmd.expression.cluster_drop.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_drop.cluster_name.len =
            name_stmt.statement.bulk.len;
        return cmd;
    }

    case CLUSTER_SET: {
        Statement name_stmt;
        Statement key_stmt;
        Statement val_stmt;
        if (astmt->len != 4) {
            printf("cluster set expects len of 4\n");
            cmd.type = INV;
            return cmd;
        }
        name_stmt = astmt->statements[1];
        key_stmt = astmt->statements[2];
        val_stmt = astmt->statements[3];

        if (name_stmt.type != SBULK) {
            printf("invalid name type, must be bulk string\n");
            cmd.type = INV;
            return cmd;
        }
        if (key_stmt.type != SBULK) {
            printf("invalid key type, must be bulk string\n");
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_set.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_set.cluster_name.len =
            name_stmt.statement.bulk.len;
        cmd.expression.cluster_set.set.key.value = key_stmt.statement.bulk.str;
        cmd.expression.cluster_set.set.key.len = key_stmt.statement.bulk.len;

        if (val_stmt.type == SBULK) {
            cmd.expression.cluster_set.set.value.type = VTSTRING;
            cmd.expression.cluster_set.set.value.size =
                val_stmt.statement.bulk.len;
            cmd.expression.cluster_set.set.value.ptr =
                val_stmt.statement.bulk.str;
        } else if (val_stmt.type == SINT) {
            cmd.expression.cluster_set.set.value.type = VTINT;
            cmd.expression.cluster_set.set.value.size = 8;
            cmd.expression.cluster_set.set.value.ptr =
                ((void*)(val_stmt.statement.i64));
        } else {
            cmd.type = INV;
            return cmd;
        }
        return cmd;
    }

    case CLUSTER_GET: {
        Statement name_stmt;
        Statement key_stmt;
        if (astmt->len != 3) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];
        key_stmt = astmt->statements[2];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        if (key_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_get.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_get.cluster_name.len =
            name_stmt.statement.bulk.len;

        cmd.expression.cluster_get.get.key.value = key_stmt.statement.bulk.str;
        cmd.expression.cluster_get.get.key.len = key_stmt.statement.bulk.len;
        return cmd;
    }

    case CLUSTER_DEL: {
        Statement name_stmt;
        Statement key_stmt;
        if (astmt->len != 3) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];
        key_stmt = astmt->statements[2];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        if (key_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_del.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_del.cluster_name.len =
            name_stmt.statement.bulk.len;

        cmd.expression.cluster_del.del.key.value = key_stmt.statement.bulk.str;
        cmd.expression.cluster_del.del.key.len = key_stmt.statement.bulk.len;
        return cmd;
    }

    case CLUSTER_PUSH: {
        Statement name_stmt;
        Statement val_stmt;
        if (astmt->len != 3) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];
        val_stmt = astmt->statements[2];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_push.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_push.cluster_name.len =
            name_stmt.statement.bulk.len;

        if (val_stmt.type == SBULK) {
            cmd.expression.cluster_push.push.value.type = VTSTRING;
            cmd.expression.cluster_push.push.value.size =
                val_stmt.statement.bulk.len;
            cmd.expression.cluster_push.push.value.ptr = val_stmt.statement.bulk.str;
        } else if (val_stmt.type == SINT) {
            cmd.expression.cluster_push.push.value.type = VTINT;
            cmd.expression.cluster_push.push.value.size = 8;
            cmd.expression.cluster_push.push.value.ptr =
                ((void*)(val_stmt.statement.i64));
        } else {
            cmd.type = INV;
            return cmd;
        }
        return cmd;
    }

    case CLUSTER_POP: {
        Statement name_stmt;
        if (astmt->len != 2) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_pop.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_pop.cluster_name.len =
            name_stmt.statement.bulk.len;

        return cmd;
    }

    case CLUSTER_KEYS: {
        Statement name_stmt;
        if (astmt->len != 2) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_keys.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_keys.cluster_name.len =
            name_stmt.statement.bulk.len;

        return cmd;
    }

    case CLUSTER_VALUES: {
        Statement name_stmt;
        if (astmt->len != 2) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_values.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_values.cluster_name.len =
            name_stmt.statement.bulk.len;

        return cmd;
    }

    case CLUSTER_ENTRIES: {
        Statement name_stmt;
        if (astmt->len != 2) {
            cmd.type = INV;
            return cmd;
        }

        name_stmt = astmt->statements[1];

        if (name_stmt.type != SBULK) {
            cmd.type = INV;
            return cmd;
        }

        cmd.expression.cluster_entries.cluster_name.value =
            name_stmt.statement.bulk.str;
        cmd.expression.cluster_entries.cluster_name.len =
            name_stmt.statement.bulk.len;

        return cmd;
    }

    default:
        cmd.type = INV;
        return cmd;
    }
}

Cmd cmd_from_statement(Statement* stmt) {
    Cmd cmd = {0};
    switch (stmt->type) {
    case SARR:
        cmd = cmd_from_array(&(stmt->statement.arr));
        return cmd;
    case SBULK:
        cmd.type =
            cmd_from_bulk(stmt->statement.bulk.str, stmt->statement.bulk.len);
        return cmd;
    case SPING:
        cmd.type = CPING;
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

void cmd_print(Cmd* cmd) {
    if (cmd->type == SET) {
        printf("SET ");
        print_key(&(cmd->expression.set.key));
        if (cmd->expression.set.value.type == VTSTRING) {
            printf("->%p(%lu bytes)\n", cmd->expression.set.value.ptr,
                   cmd->expression.set.value.size);
        } else if (cmd->expression.set.value.type == VTINT) {
            printf("->%ld\n", ((int64_t)cmd->expression.set.value.ptr));
        }
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
    if (cmd->type == CPING) {
        printf("PING\n");
    }
    if (cmd->type == PUSH) {
        printf("PUSH ");
        if (cmd->expression.push.value.type == VTSTRING) {
            printf("->%p(%lu bytes)\n", cmd->expression.push.value.ptr,
                   cmd->expression.push.value.size);
        } else if (cmd->expression.push.value.type == VTINT) {
            printf("->%ld\n", ((int64_t)cmd->expression.push.value.ptr));
        }
    }
    if (cmd->type == POP) {
        printf("POP\n");
    }
}
