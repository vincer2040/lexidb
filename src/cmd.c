#include "cmd.h"
#include "parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CmdT cmd_from_bulk(uint8_t* str, size_t str_len) {
    switch (str_len) {
    case 2: {
        if (memcmp(str, "HT", 2) == 0) {
            return HT;
        }
        return INV;
    }
    case 3: {

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

    case 4: {

        if (memcmp(str, "PUSH", 4) == 0) {
            return PUSH;
        }

        if (memcmp(str, "KEYS", 4) == 0) {
            return KEYS;
        }

        return INV;
    }

    case 5: {

        if (memcmp(str, "ENQUE", 5) == 0) {
            return ENQUE;
        }

        if (memcmp(str, "DEQUE", 5) == 0) {
            return DEQUE;
        }

        if (memcmp(str, "STACK", 5) == 0) {
            return STACK;
        }

        if (memcmp(str, "QUEUE", 5) == 0) {
            return QUEUE;
        }

        return INV;
    }

    case 6: {
        if (memcmp(str, "VALUES", 6) == 0) {
            return VALUES;
        }

        return INV;
    }

    case 7: {
        if (memcmp(str, "ENTRIES", 7) == 0) {
            return ENTRIES;
        }

        if (memcmp(str, "CLUSTER", 7) == 0) {
            return CLUSTER;
        }

        return INV;
    }

    case 9: {
        if (memcmp(str, "REPLICATE", 9) == 0) {
            return REPLICATE;
        }

        return INV;
    }

    case 11: {
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

        if (memcmp(str, "REPLICATION", 11) == 0) {
            return REPLICATION;
        }

        return INV;
    }

    case 12: {
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

    case 14: {
        if (memcmp(str, "CLUSTER.VALUES", 14) == 0) {
            return CLUSTER_VALUES;
        }

        return INV;
    }

    case 15: {
        if (memcmp(str, "CLUSTER.ENTRIES", 15) == 0) {
            return CLUSTER_ENTRIES;
        }

        return INV;
    }

    default:
        return INV;
    }
}

CmdT cmdt_from_statement(Statement* stmt) {
    StatementType type = stmt->type;

    switch (type) {
    case SBULK:
        return cmd_from_bulk(stmt->statement.bulk.str,
                             stmt->statement.bulk.len);
    case SARR:
        return MULTI_CMD;
    default:
        return INV;
    }
}

/* TODO: remove the asserts and have proper error handling */
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

    case ENQUE: {
        Statement val_stmt;
        if (astmt->len != 2) {
            printf("enque expects len of 2, got %lu, cmd, value\n", astmt->len);
            cmd.type = INV;
            return cmd;
        }

        val_stmt = astmt->statements[1];
        if (val_stmt.type == SBULK) {
            cmd.expression.enque.value.type = VTSTRING;
            cmd.expression.enque.value.size = val_stmt.statement.bulk.len;
            cmd.expression.enque.value.ptr = val_stmt.statement.bulk.str;
            return cmd;
        } else if (val_stmt.type == SINT) {
            cmd.expression.enque.value.type = VTINT;
            cmd.expression.enque.value.size = 8;
            cmd.expression.enque.value.ptr = ((void*)(val_stmt.statement.i64));
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
            cmd.expression.cluster_push.push.value.ptr =
                val_stmt.statement.bulk.str;
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

    case MULTI_CMD: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        size_t i, num_cmds = astmt->len;
        multi.commands = calloc(num_cmds, sizeof(struct Cmd));
        assert(multi.commands != NULL);
        for (i = 0; i < num_cmds; ++i) {
            Statement stmt = astmt->statements[i];
            multi.commands[i] = cmd_from_statement(&stmt);
        }
        multi.len = num_cmds;
        cmd.type = MULTI_CMD;
        cmd.expression.multi = multi;
        return cmd;
    }

    case REPLICATION: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        Statement replication_stmt = astmt->statements[1];
        size_t i, len;
        assert(replication_stmt.type == SARR);
        assert(replication_stmt.statement.arr.len == 4);

        len = replication_stmt.statement.arr.len;
        multi.commands = calloc(len, sizeof(struct Cmd));
        multi.len = len;
        for (i = 0; i < len; ++i) {
            Statement cur = replication_stmt.statement.arr.statements[i];
            multi.commands[i] = cmd_from_statement(&cur);
        }
        cmd.expression.multi = multi;
        cmd.type = REPLICATION;
        return cmd;
    }

    case HT: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        Statement ht_entries = astmt->statements[1];
        size_t i, len;
        assert(ht_entries.type == SARR);
        len = ht_entries.statement.arr.len;

        if (len == 0) {
            cmd.type = HT;
            cmd.expression.multi = multi;
            return cmd;
        }

        multi.len = len;
        multi.commands = calloc(len, sizeof(struct Cmd));

        for (i = 0; i < len; ++i) {
            Statement cur_entry = ht_entries.statement.arr.statements[i];
            Statement key_stmt;
            Statement val_stmt;
            Cmd set_cmd = {0};
            assert(cur_entry.type == SARR);
            assert(cur_entry.statement.arr.len == 2);
            key_stmt = cur_entry.statement.arr.statements[0];
            assert(key_stmt.type == SBULK);
            val_stmt = cur_entry.statement.arr.statements[1];
            set_cmd.type = SET;
            set_cmd.expression.set.key.len = key_stmt.statement.bulk.len;
            set_cmd.expression.set.key.value = key_stmt.statement.bulk.str;
            if (val_stmt.type == SBULK) {
                set_cmd.expression.set.value.type = VTSTRING;
                set_cmd.expression.set.value.size = val_stmt.statement.bulk.len;
                set_cmd.expression.set.value.ptr = val_stmt.statement.bulk.str;
            } else if (val_stmt.type == SINT) {
                set_cmd.expression.set.value.type = VTINT;
                set_cmd.expression.set.value.size = 8;
                set_cmd.expression.set.value.ptr =
                    ((void*)(val_stmt.statement.i64));
            } else {
                free(multi.commands);
                cmd.type = INV;
                return cmd;
            }

            multi.commands[i] = set_cmd;
        }
        cmd.type = HT;
        cmd.expression.multi = multi;
        return cmd;
    }

    case STACK: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        Statement stack_entries = astmt->statements[1];
        size_t i, len;
        assert(stack_entries.type == SARR);
        len = stack_entries.statement.arr.len;
        if (len == 0) {
            cmd.type = STACK;
            cmd.expression.multi = multi;
            return cmd;
        }
        multi.len = len;
        multi.commands = calloc(len, sizeof(struct Cmd));
        for (i = 0; i < len; ++i) {
            Statement val_stmt = stack_entries.statement.arr.statements[i];
            Cmd push_cmd = {0};
            push_cmd.type = PUSH;
            if (val_stmt.type == SBULK) {
                push_cmd.expression.push.value.type = VTSTRING;
                push_cmd.expression.push.value.size =
                    val_stmt.statement.bulk.len;
                push_cmd.expression.push.value.ptr =
                    val_stmt.statement.bulk.str;
            } else if (val_stmt.type == SINT) {
                push_cmd.expression.push.value.type = VTINT;
                push_cmd.expression.push.value.size = 8;
                push_cmd.expression.push.value.ptr =
                    ((void*)(val_stmt.statement.i64));
            } else {
                cmd.type = INV;
                free(multi.commands);
                return cmd;
            }
            multi.commands[i] = push_cmd;
        }

        cmd.type = STACK;
        cmd.expression.multi = multi;

        return cmd;
    }

    case QUEUE: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        Statement stack_entries = astmt->statements[1];
        size_t i, len;
        assert(stack_entries.type == SARR);
        len = stack_entries.statement.arr.len;
        if (len == 0) {
            cmd.type = STACK;
            cmd.expression.multi = multi;
            return cmd;
        }
        multi.len = len;
        multi.commands = calloc(len, sizeof(struct Cmd));
        for (i = 0; i < len; ++i) {
            Statement val_stmt = stack_entries.statement.arr.statements[i];
            Cmd enque_cmd = {0};
            enque_cmd.type = ENQUE;
            if (val_stmt.type == SBULK) {
                enque_cmd.expression.push.value.type = VTSTRING;
                enque_cmd.expression.push.value.size =
                    val_stmt.statement.bulk.len;
                enque_cmd.expression.push.value.ptr =
                    val_stmt.statement.bulk.str;
            } else if (val_stmt.type == SINT) {
                enque_cmd.expression.push.value.type = VTINT;
                enque_cmd.expression.push.value.size = 8;
                enque_cmd.expression.push.value.ptr =
                    ((void*)(val_stmt.statement.i64));
            } else {
                cmd.type = INV;
                free(multi.commands);
                return cmd;
            }
            multi.commands[i] = enque_cmd;
        }

        cmd.type = QUEUE;
        cmd.expression.multi = multi;

        return cmd;
    }

    case CLUSTER: {
        Cmd cmd = {0};
        MultiCmd multi = {0};
        Statement cluster_statements = astmt->statements[1];
        size_t i, len;
        assert(cluster_statements.type == SARR);
        len = cluster_statements.statement.arr.len;

        multi.len = len;
        if (len > 0) {
            multi.commands = calloc(len, sizeof(struct Cmd));
            for (i = 0; i < len; ++i) {
                Cmd cur_cmd = {0};
                Cmd cluster_new_cmd = {0};
                MultiCmd cur_multi = {0};
                Statement name_stmt, full_ht_stmt, full_stack_stmt, ht_stmt,
                    stack_stmt,
                    cluster_stmt =
                        cluster_statements.statement.arr.statements[i];
                Key cluster_name = {0};
                size_t k, multi_len, multi_ins, num_ht_entries,
                    num_stack_entries;
                assert(cluster_stmt.type == SARR);
                assert(cluster_stmt.statement.arr.len == 3);
                name_stmt = cluster_stmt.statement.arr.statements[0];
                assert(name_stmt.type == SBULK);
                cluster_name.len = name_stmt.statement.bulk.len;
                cluster_name.value = name_stmt.statement.bulk.str;

                full_ht_stmt = cluster_stmt.statement.arr.statements[1];
                assert(full_ht_stmt.type == SARR);
                assert(full_ht_stmt.statement.arr.len == 2);
                full_stack_stmt = cluster_stmt.statement.arr.statements[2];
                assert(full_stack_stmt.type == SARR);
                assert(full_stack_stmt.statement.arr.len == 2);

                ht_stmt = full_ht_stmt.statement.arr.statements[1];
                assert(ht_stmt.type == SARR);
                num_ht_entries = ht_stmt.statement.arr.len;
                stack_stmt = full_stack_stmt.statement.arr.statements[1];
                assert(stack_stmt.type == SARR);
                num_stack_entries = stack_stmt.statement.arr.len;

                cluster_new_cmd.type = CLUSTER_NEW;
                cluster_new_cmd.expression.cluster_new.cluster_name =
                    cluster_name;

                multi_len = 1 + num_ht_entries + num_stack_entries;

                cur_multi.commands = calloc(multi_len, sizeof(struct Cmd));
                cur_multi.len = multi_len;

                cur_multi.commands[0] = cluster_new_cmd;
                multi_ins = 1;

                for (k = 0; k < num_ht_entries; ++k) {
                    Cmd cluster_set_cmd = {0};
                    Statement key_stmt, val_stmt,
                        entry_stmt = ht_stmt.statement.arr.statements[k];

                    assert(entry_stmt.type == SARR);
                    assert(entry_stmt.statement.arr.len == 2);
                    key_stmt = entry_stmt.statement.arr.statements[0];
                    assert(key_stmt.type == SBULK);
                    val_stmt = entry_stmt.statement.arr.statements[1];

                    cluster_set_cmd.expression.cluster_set.cluster_name =
                        cluster_name;
                    cluster_set_cmd.expression.cluster_set.set.key.len =
                        key_stmt.statement.bulk.len;
                    cluster_set_cmd.expression.cluster_set.set.key.value =
                        key_stmt.statement.bulk.str;

                    if (val_stmt.type == SBULK) {
                        cluster_set_cmd.expression.cluster_set.set.value.type =
                            VTSTRING;
                        cluster_set_cmd.expression.cluster_set.set.value.size =
                            val_stmt.statement.bulk.len;
                        cluster_set_cmd.expression.cluster_set.set.value.ptr =
                            val_stmt.statement.bulk.str;
                    } else if (val_stmt.type == SINT) {
                        cluster_set_cmd.expression.cluster_set.set.value.type =
                            VTINT;
                        cluster_set_cmd.expression.cluster_set.set.value.size =
                            8;
                        cluster_set_cmd.expression.cluster_set.set.value.ptr =
                            ((void*)(val_stmt.statement.i64));
                    } else {
                        assert(0 && "invalid value");
                    }

                    cluster_set_cmd.type = CLUSTER_SET;
                    cur_multi.commands[multi_ins] = cluster_set_cmd;
                    multi_ins++;
                }

                for (k = 0; k < num_stack_entries; ++k) {
                    Cmd cluster_push_cmd = {0};
                    Statement val_stmt = stack_stmt.statement.arr.statements[k];
                    if (val_stmt.type == SBULK) {
                        cluster_push_cmd.expression.cluster_push.push.value
                            .type = VTSTRING;
                        cluster_push_cmd.expression.cluster_push.push.value
                            .size = val_stmt.statement.bulk.len;
                        cluster_push_cmd.expression.cluster_push.push.value
                            .ptr = val_stmt.statement.bulk.str;
                    } else if (val_stmt.type == SINT) {
                        cluster_push_cmd.expression.cluster_push.push.value
                            .type = VTINT;
                        cluster_push_cmd.expression.cluster_push.push.value
                            .size = 8;
                        cluster_push_cmd.expression.cluster_push.push.value
                            .ptr = ((void*)(val_stmt.statement.i64));
                    } else {
                        assert(0 && "invalid value");
                    }

                    cluster_push_cmd.type = CLUSTER_PUSH;
                    cur_multi.commands[multi_ins] = cluster_push_cmd;
                    multi_ins++;
                }

                cur_cmd.expression.multi = cur_multi;
                cur_cmd.type = MULTI_CMD;
                multi.commands[i] = cur_cmd;
            }
        }

        cmd.type = CLUSTER;
        cmd.expression.multi = multi;
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
    case SOK:
        cmd.type = COK;
        return cmd;
    default:
        cmd.type = INV;
        return cmd;
    }
}

void cmd_free(Cmd* cmd) {
    if (cmd->type == MULTI_CMD || cmd->type == HT || cmd->type == STACK ||
        cmd->type == QUEUE || cmd->type == CLUSTER ||
        cmd->type == REPLICATION) {
        size_t i, len = cmd->expression.multi.len;
        for (i = 0; i < len; ++i) {
            Cmd cur = cmd->expression.multi.commands[i];
            cmd_free(&cur);
        }
        if (len > 0) {
            free(cmd->expression.multi.commands);
        }
    } else {
        return;
    }
}

int is_write_command(CmdT type) {
    int res = 0;
    switch (type) {
    case SET:
    case DEL:
    case PUSH:
    case POP:
    case ENQUE:
    case DEQUE:
    case CLUSTER_NEW:
    case CLUSTER_DROP:
    case CLUSTER_SET:
    case CLUSTER_DEL:
    case CLUSTER_PUSH:
    case CLUSTER_POP:
        res = 1;
        break;
    default:
        break;
    }
    return res;
}
