#include "log.h"
#include "cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void slowlog(uint8_t* buf, size_t len) {
    size_t i;
    LOG(LOG_MSG);
    for (i = 0; i < len; ++i) {
        char at = buf[i];
        printf("%x ", at);

        // if we don't branch like this it logs the
        // wrong hex values. It might be because of
        // the 0 values in the string in the int
        // but I am not a genius
        if (at == ':') {
            size_t k;
            i++;
            for (k = 0; k < 8; ++k, ++i) {
                printf("%x ", buf[i]);
            }
            i--;
        }
    }
    printf("\n");
    fflush(stdout);
}

void log_cmd(Cmd* cmd) {
    Key name;
    Key key;
    Value v;
    CmdT t;
    size_t i, key_len, name_len;

    LOG(LOG_CMD);

    t = cmd->type;

    switch (t) {
    case INV:
        printf("INVALID\n");
        return;
        break;

    case CPING:
        printf("PING\n");
        break;

    case OK:
        printf("OK\n");
        break;

    case SET:
        key = cmd->expression.set.key;
        v = cmd->expression.set.value;
        printf("SET ");
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }

        printf(" ");

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%ld", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
        break;

    case GET:
        printf("GET ");
        key = cmd->expression.get.key;
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case DEL:
        printf("DEL ");
        key = cmd->expression.del.key;
        key_len = key.len;
        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case PUSH:
        printf("PUSH ");
        v = cmd->expression.push.value;

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
        break;

    case POP:
        printf("POP\n");
        fflush(stdout);
        break;

    case ENQUE:
        printf("ENQUE ");
        v = cmd->expression.enque.value;

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
        break;

    case DEQUE:
        printf("DEQUE\n");
        break;

    case KEYS:
        printf("KEYS\n");
        fflush(stdout);
        break;

    case VALUES:
        printf("VALUES\n");
        fflush(stdout);
        break;

    case ENTRIES:
        printf("ENTRIES\n");
        fflush(stdout);
        break;

    case CLUSTER_NEW:
        printf("CLUSTER_NEW ");
        name = cmd->expression.cluster_new.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_DROP:
        printf("CLUSTER_DROP ");
        name = cmd->expression.cluster_drop.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_SET:
        printf("CLUSTER_SET ");
        name = cmd->expression.cluster_set.cluster_name;
        name_len = name.len;
        key = cmd->expression.cluster_set.set.key;
        key_len = key.len;
        v = cmd->expression.cluster_set.set.value;

        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }

        printf(" ");

        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }

        printf(" ");

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_GET:
        printf("CLUSTER_GET ");
        name = cmd->expression.cluster_get.cluster_name;
        name_len = name.len;
        key = cmd->expression.cluster_get.get.key;
        key_len = key.len;

        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }

        printf(" ");

        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }

        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_DEL:
        printf("CLUSTER_DEL ");
        name = cmd->expression.cluster_del.cluster_name;
        name_len = name.len;
        key = cmd->expression.cluster_del.del.key;
        key_len = key.len;

        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }

        printf(" ");

        for (i = 0; i < key_len; ++i) {
            printf("%c", key.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_PUSH:
        printf("CLUSTER_PUSH ");
        name = cmd->expression.cluster_push.cluster_name;
        name_len = name.len;
        v = cmd->expression.cluster_push.push.value;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }

        printf(" ");

        if (v.type == VTSTRING) {
            size_t v_len;
            char* val = ((char*)(v.ptr));
            v_len = v.size;

            for (i = 0; i < v_len; ++i) {
                printf("%c", val[i]);
            }
        } else if (v.type == VTINT) {
            printf("%lu", ((int64_t)v.ptr));
        } else {
            printf("null");
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_POP:
        printf("CLUSTER_POP ");
        name = cmd->expression.cluster_pop.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_KEYS:
        printf("CLUSTER_KEYS ");
        name = cmd->expression.cluster_keys.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_VALUES:
        printf("CLUSTER_KEYS ");
        name = cmd->expression.cluster_values.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;

    case CLUSTER_ENTRIES:
        printf("CLUSTER_KEYS ");
        name = cmd->expression.cluster_entries.cluster_name;
        name_len = name.len;
        for (i = 0; i < name_len; ++i) {
            printf("%c", name.value[i]);
        }
        printf("\n");
        fflush(stdout);
        break;
    case MULTI_CMD: {
        printf("MULTI_COMMAND len: %lu\n", cmd->expression.multi.len);
    } break;
    case REPLICATE:
        printf("REPLICATE\n");
    }
}
