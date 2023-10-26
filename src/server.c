#include "server.h"
#include "builder.h"
#include "cluster.h"
#include "cmd.h"
#include "config.h"
#include "de.h"
#include "ht.h"
#include "lexer.h"
#include "log.h"
#include "objects.h"
#include "parser.h"
#include "queue.h"
#include "sock.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HT_INITIAL_CAP 32
#define YES_NONBLOCK 1
#define BACKLOG 10
#define MAX_READ 4096
#define CONN_BUF_INIT_CAP 4096

void read_from_client(De* de, int fd, void* client_data, uint32_t flags);
void lexidb_free(LexiDB* db);
void vec_free_cb(void* ptr);
void free_cb(void* ptr);

LexiDB* lexidb_new() {
    LexiDB* db;
    db = calloc(1, sizeof *db);

    if (db == NULL) {
        return NULL;
    }

    db->ht = ht_new(HT_INITIAL_CAP, sizeof(Object));
    if (db->ht == NULL) {
        free(db);
        return NULL;
    }

    db->cluster = cluster_new(HT_INITIAL_CAP);
    if (db->cluster == NULL) {
        ht_free(db->ht, free_cb);
        free(db);
        return NULL;
    }

    db->stack = vec_new(32, sizeof(Object));
    if (db->stack == NULL) {
        ht_free(db->ht, free_cb);
        cluster_free(db->cluster);
        free(db);
        return NULL;
    }

    db->queue = queue_new(sizeof(Object));
    if (db->queue == NULL) {
        ht_free(db->ht, free_cb);
        cluster_free(db->cluster);
        vec_free(db->stack, vec_free_cb);
        free(db);
        return NULL;
    }

    return db;
}

Server* server_create(int sfd, LogLevel loglevel, int isreplica) {
    Server* server;

    if (sfd < 0) {
        return NULL;
    }

    server = calloc(1, sizeof *server);
    if (server == NULL) {
        return NULL;
    }

    server->sfd = sfd;
    server->loglevel = loglevel;

    server->db = lexidb_new();
    if (server->db == NULL) {
        free(server);
        close(sfd);
        return NULL;
    }

    server->clients = vec_new(32, sizeof(Client*));
    if (server->clients == NULL) {
        lexidb_free(server->db);
        free(server);
        close(sfd);
        return NULL;
    }

    if (isreplica == -1) {
        server->ismaster = 1;
    } else {
        server->isslave = 1;
    }

    return server;
}

Connection* connection_create(uint32_t addr, uint16_t port) {
    Connection* conn;

    conn = calloc(1, sizeof *conn);
    if (conn == NULL) {
        return NULL;
    }

    conn->addr = addr;
    conn->port = port;

    conn->read_buf = calloc(CONN_BUF_INIT_CAP, sizeof(uint8_t));
    if (conn->read_buf == NULL) {
        free(conn);
        return NULL;
    }

    conn->read_pos = 0;
    conn->read_cap = CONN_BUF_INIT_CAP;
    conn->flags = 0;

    return conn;
}

Client* client_create(Connection* conn, LexiDB* db, int fd) {
    Client* client;

    client = calloc(1, sizeof *client);
    if (client == NULL) {
        return NULL;
    }

    client->fd = fd;
    client->conn = conn;
    client->db = db;
    client->builder = builder_create(32);
    return client;
}

void free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
    free(obj);
}

void vec_free_cb(void* ptr) {
    Object* obj = ((Object*)ptr);
    object_free(obj);
}

void connection_close(De* de, Client* client, int fd, uint32_t flags,
                      LogLevel loglevel) {
    Connection* conn = client->conn;
    if (loglevel >= LL_INFO) {
        LOG(LOG_CLOSE "fd: %d, addr: %u, port: %u\n", fd, conn->addr,
            conn->port);
    }
    if (conn->read_buf) {
        free(conn->read_buf);
        conn->read_buf = NULL;
    }
    builder_free(&(client->builder));
    free(conn);
    conn = NULL;
    if (de_del_event(de, fd, flags) == -1) {
        fmt_error("error deleting event\n");
        return;
    }
    close(fd);
}

int remove_client_from_vec(void* a, void* b) {
    Client* ac = ((Client*)a);
    Client* bc = *((Client**)b);
    return ac->fd - bc->fd;
}

void client_close(De* de, Server* s, Client* client, int fd, uint32_t flags) {
    vec_remove(s->clients, client, remove_client_from_vec);
    connection_close(de, client, fd, flags, s->loglevel);
    free(client);
}

void lexidb_free(LexiDB* db) {
    ht_free(db->ht, free_cb);
    cluster_free(db->cluster);
    vec_free(db->stack, vec_free_cb);
    queue_free(db->queue, vec_free_cb);
    free(db);
}

/**
 * TODO: send message to clients
 * telling them we are disconnecting
 * due to server shutdown
 */
void vec_free_client_cb(void* ptr) {
    Client* client = *((Client**)ptr);
    if (client->conn->read_buf) {
        free(client->conn->read_buf);
    }
    builder_free(&(client->builder));
    free(client->conn);
    free(client);
}

void server_destroy(Server* server) {
    close(server->sfd);
    lexidb_free(server->db);
    vec_free(server->clients, vec_free_client_cb);
    free(server);
    server = NULL;
}

/**
 * run the command sent to the server
 * the command is valid if it is passed to
 * this function, so there is no checking.
 * all commands are in this function
 * to avoid jumping around the whole file.
 */
void evaluate_cmd(Cmd* cmd, Client* client, LogLevel loglevel,
                  Builder* builder) {
    CmdT cmd_type;
    Connection* conn = client->conn;

    if (loglevel >= LL_CMD) {
        log_cmd(cmd);
    }

    cmd_type = cmd->type;

    switch (cmd_type) {
    case INV: {
        builder_add_err(builder, ((uint8_t*)"invalid command"), 15);
        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;
    case CPING: {
        builder_add_pong(builder);
        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case COK:
        return;

    case SET: {
        Object obj;
        int set_res;
        SetCmd set_cmd;
        uint8_t* key;
        size_t key_len;
        void* value;
        size_t value_size;

        set_cmd = cmd->expression.set;
        key = set_cmd.key.value;
        key_len = set_cmd.key.len;
        value = set_cmd.value.ptr;
        value_size = set_cmd.value.size;

        if (set_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, free_cb);
        } else if (set_cmd.value.type == VTINT) {
            obj = object_new(OINT64, value, value_size);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, free_cb);
        } else {
            obj = object_new(ONULL, NULL, 0);
            set_res = ht_insert(client->db->ht, key, key_len, &obj, free_cb);
        }

        if (set_res != 0) {
            uint8_t* e = ((uint8_t*)"could not set");
            size_t n = strlen((char*)e);
            builder_add_err(builder, e, n);
        } else {
            builder_add_ok(builder);
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case GET: {
        GetCmd get_cmd;
        uint8_t* key;
        size_t key_len;
        void* get_res;

        get_cmd = cmd->expression.get;
        key = get_cmd.key.value;
        key_len = get_cmd.key.len;
        get_res = ht_get(client->db->ht, key, key_len);

        if (get_res == NULL) {
            builder_add_none(builder);
        } else {
            Object* obj = ((Object*)get_res);
            vstr str;
            if (obj->type == STRING) {
                size_t len;
                str = obj->data.str;
                len = vstr_len(obj->data.str);
                builder_add_string(builder, str, len);
            } else if (obj->type == OINT64) {
                int64_t i;
                i = obj->data.i64;
                builder_add_int(builder, i);
            } else {
                builder_add_none(builder);
            }
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case DEL: {
        DelCmd del_cmd;
        uint8_t* key;
        size_t key_len;

        del_cmd = cmd->expression.del;
        key = del_cmd.key.value;
        key_len = del_cmd.key.len;

        ht_delete(client->db->ht, key, key_len, free_cb);
        builder_add_ok(builder);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case PUSH: {
        Object obj;
        PushCmd push_cmd;
        int push_res;
        void* value;
        size_t value_size;

        push_cmd = cmd->expression.push;
        value = push_cmd.value.ptr;
        value_size = push_cmd.value.size;

        if (push_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
        } else if (push_cmd.value.type == VTINT) {
            obj = object_new(OINT64, value, value_size);
        } else {
            obj = object_new(ONULL, NULL, 0);
        }

        push_res = vec_push(&(client->db->stack), &obj);
        if (push_res != 0) {
            uint8_t* e = ((uint8_t*)"could not push");
            size_t n = strlen((char*)e);
            builder_add_err(builder, e, n);
        } else {
            builder_add_ok(builder);
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case POP: {
        Object obj;
        int pop_res;

        pop_res = vec_pop(client->db->stack, &obj);
        if (pop_res == -1) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        if (obj.type == ONULL) {
            builder_add_none(builder);
        }

        if (obj.type == OINT64) {
            builder_add_int(builder, obj.data.i64);
        }

        if (obj.type == STRING) {
            size_t len = vstr_len(obj.data.str);
            builder_add_string(builder, obj.data.str, len);
            vstr_delete(obj.data.str);
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case ENQUE: {
        Object obj;
        EnqueCmd enque_cmd;
        int enque_res;
        void* value;
        size_t value_size;

        enque_cmd = cmd->expression.enque;
        value = enque_cmd.value.ptr;
        value_size = enque_cmd.value.size;

        if (enque_cmd.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
        } else if (enque_cmd.value.type == VTINT) {
            obj = object_new(OINT64, value, value_size);
        } else {
            obj = object_new(ONULL, NULL, 0);
        }

        enque_res = queue_enque(client->db->queue, &obj);

        if (enque_res != 0) {
            uint8_t* e = (uint8_t*)"could not enque";
            size_t n = strlen((char*)e);
            builder_add_err(builder, e, n);
        } else {
            builder_add_ok(builder);
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case DEQUE: {
        Object obj;
        int deque_res;

        deque_res = queue_deque(client->db->queue, &obj);
        if (deque_res == -1) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        if (obj.type == ONULL) {
            builder_add_none(builder);
        }

        if (obj.type == OINT64) {
            builder_add_int(builder, obj.data.i64);
        }

        if (obj.type == STRING) {
            size_t len = vstr_len(obj.data.str);
            builder_add_string(builder, obj.data.str, len);
            vstr_delete(obj.data.str);
        }

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case KEYS: {
        HtEntriesIter* iter;
        Entry* cur;

        if (client->db->ht->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        builder_add_arr(builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            builder_add_string(builder, ((char*)cur->key), cur->key_len);
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;

    } break;

    case VALUES: {
        HtEntriesIter* iter;
        Entry* cur;

        if (client->db->ht->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        builder_add_arr(builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            Object* obj = ((Object*)cur->value);
            if (obj->type == OINT64) {
                builder_add_int(builder, obj->data.i64);
            } else if (obj->type == STRING) {
                size_t len = vstr_len(obj->data.str);
                builder_add_string(builder, obj->data.str, len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;

    } break;

    case ENTRIES: {
        HtEntriesIter* iter;
        Entry* cur;

        if (client->db->ht->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        iter = ht_entries_iter(client->db->ht);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        builder_add_arr(builder, client->db->ht->len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            uint8_t* key = cur->key;
            size_t key_len = cur->key_len;
            Object* obj = ((Object*)cur->value);
            builder_add_arr(builder, 2);
            builder_add_string(builder, ((char*)key), key_len);
            if (obj->type == OINT64) {
                builder_add_int(builder, obj->data.i64);
            } else if (obj->type == STRING) {
                size_t len = vstr_len(obj->data.str);
                builder_add_string(builder, obj->data.str, len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;

    } break;

    case CLUSTER_NEW: {
        ClusterNewCmd cluster_new_cmd;
        uint8_t* name;
        size_t name_len;
        int create_res;

        cluster_new_cmd = cmd->expression.cluster_new;

        name = cluster_new_cmd.cluster_name.value;
        name_len = cluster_new_cmd.cluster_name.len;

        create_res = cluster_namespace_new(client->db->cluster, name, name_len,
                                           HT_INITIAL_CAP);
        if (create_res == 1) {
            builder_add_err(builder, ((uint8_t*)"cluster name already taken"),
                            18);
        } else if (create_res == -1) {
            builder_add_err(builder, ((uint8_t*)"failed to create cluster"),
                            24);
        } else {
            builder_add_ok(builder);
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_DROP: {
        ClusterDropCmd cluster_drop_cmd;
        uint8_t* name;
        size_t name_len;
        int drop_res;

        cluster_drop_cmd = cmd->expression.cluster_drop;

        name = cluster_drop_cmd.cluster_name.value;
        name_len = cluster_drop_cmd.cluster_name.len;

        drop_res = cluster_namespace_drop(client->db->cluster, name, name_len);
        if (drop_res == -1) {
            builder_add_err(builder, ((uint8_t*)"invalid name"), 12);
        } else {
            builder_add_ok(builder);
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_SET: {
        ClusterSetCmd cluster_set_cmd;
        uint8_t *name, *key;
        void* value;
        size_t name_len, key_len, value_size;
        int set_res;
        Object obj;

        cluster_set_cmd = cmd->expression.cluster_set;

        name = cluster_set_cmd.cluster_name.value;
        name_len = cluster_set_cmd.cluster_name.len;
        key = cluster_set_cmd.set.key.value;
        key_len = cluster_set_cmd.set.key.len;
        value = cluster_set_cmd.set.value.ptr;
        value_size = cluster_set_cmd.set.value.size;

        if (cluster_set_cmd.set.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
            set_res =
                cluster_namespace_insert(client->db->cluster, name, name_len,
                                         key, key_len, &obj, free_cb);
        } else if (cluster_set_cmd.set.value.type == VTINT) {
            obj = object_new(OINT64, value, value_size);
            set_res =
                cluster_namespace_insert(client->db->cluster, name, name_len,
                                         key, key_len, &obj, free_cb);
        } else {
            obj = object_new(ONULL, NULL, 0);
            set_res =
                cluster_namespace_insert(client->db->cluster, name, name_len,
                                         key, key_len, &obj, free_cb);
        }

        if (set_res != 0) {
            uint8_t* e = ((uint8_t*)"could not set");
            size_t n = strlen((char*)e);
            builder_add_err(builder, e, n);
        } else {
            builder_add_ok(builder);
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_GET: {
        ClusterGetCmd cluster_get_cmd;
        uint8_t *name, *key;
        size_t name_len, key_len;
        void* get_res;

        cluster_get_cmd = cmd->expression.cluster_get;

        name = cluster_get_cmd.cluster_name.value;
        name_len = cluster_get_cmd.cluster_name.len;
        key = cluster_get_cmd.get.key.value;
        key_len = cluster_get_cmd.get.key.len;

        get_res = cluster_namespace_get(client->db->cluster, name, name_len,
                                        key, key_len);

        if (get_res == NULL) {
            builder_add_none(builder);
        } else {
            Object* obj = ((Object*)get_res);
            vstr str;
            if (obj->type == STRING) {
                size_t len;
                str = obj->data.str;
                len = vstr_len(obj->data.str);
                builder_add_string(builder, str, len);
            } else if (obj->type == OINT64) {
                int64_t i;
                i = obj->data.i64;
                builder_add_int(builder, i);
            } else {
                builder_add_none(builder);
            }
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_DEL: {
        ClusterDelCmd cluster_del_cmd;
        uint8_t *name, *key;
        size_t name_len, key_len;

        cluster_del_cmd = cmd->expression.cluster_del;

        name = cluster_del_cmd.cluster_name.value;
        name_len = cluster_del_cmd.cluster_name.len;
        key = cluster_del_cmd.del.key.value;
        key_len = cluster_del_cmd.del.key.len;

        cluster_namespace_del(client->db->cluster, name, name_len, key, key_len,
                              free_cb);

        builder_add_ok(builder);

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_PUSH: {
        ClusterPushCmd cluster_push_cmd;
        uint8_t* name;
        void* value;
        size_t name_len, value_size;
        int push_res;
        Object obj;

        cluster_push_cmd = cmd->expression.cluster_push;

        name = cluster_push_cmd.cluster_name.value;
        name_len = cluster_push_cmd.cluster_name.len;
        value = cluster_push_cmd.push.value.ptr;
        value_size = cluster_push_cmd.push.value.size;

        if (cluster_push_cmd.push.value.type == VTSTRING) {
            obj = object_new(STRING, value, value_size);
        } else if (cluster_push_cmd.push.value.type == VTINT) {
            obj = object_new(OINT64, value, value_size);
        } else {
            obj = object_new(ONULL, NULL, 0);
        }

        push_res =
            cluster_namespace_push(client->db->cluster, name, name_len, &obj);

        if (push_res != 0) {
            uint8_t* e = ((uint8_t*)"could not push");
            size_t n = strlen((char*)e);
            builder_add_err(builder, e, n);
        } else {
            builder_add_ok(builder);
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_POP: {
        ClusterPopCmd cluster_pop_cmd;
        uint8_t* name;
        size_t name_len;
        int pop_res;
        Object obj = {0};

        cluster_pop_cmd = cmd->expression.cluster_pop;

        name = cluster_pop_cmd.cluster_name.value;
        name_len = cluster_pop_cmd.cluster_name.len;

        pop_res =
            cluster_namespace_pop(client->db->cluster, name, name_len, &obj);

        if (pop_res == -1) {
            builder_add_none(builder);
            conn->write_size = builder->ins;
            conn->write_buf = builder_out(builder);
            return;
        }

        if (obj.type == ONULL) {
            builder_add_none(builder);
        }

        if (obj.type == OINT64) {
            builder_add_int(builder, obj.data.i64);
        }

        if (obj.type == STRING) {
            size_t len = vstr_len(obj.data.str);
            builder_add_string(builder, obj.data.str, len);
            vstr_delete(obj.data.str);
        }

        conn->write_size = builder->ins;
        conn->write_buf = builder_out(builder);
    } break;

    case CLUSTER_KEYS: {
        HtEntriesIter* iter;
        ClusterKeysCmd cluster_keys_cmd;
        Entry* cur;
        uint8_t* name;
        size_t name_len, cluster_len;

        if (client->db->cluster->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_keys_cmd = cmd->expression.cluster_keys;

        name = cluster_keys_cmd.cluster_name.value;
        name_len = cluster_keys_cmd.cluster_name.len;

        iter =
            cluster_namespace_entries_iter(client->db->cluster, name, name_len);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_len =
            cluster_namespace_len(client->db->cluster, name, name_len);

        builder_add_arr(builder, cluster_len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            builder_add_string(builder, ((char*)cur->key), cur->key_len);
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case CLUSTER_VALUES: {
        HtEntriesIter* iter;
        ClusterValuesCmd cluster_values_cmd;
        Entry* cur;
        uint8_t* name;
        size_t name_len, cluster_len;

        if (client->db->cluster->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_values_cmd = cmd->expression.cluster_values;

        name = cluster_values_cmd.cluster_name.value;
        name_len = cluster_values_cmd.cluster_name.len;

        iter =
            cluster_namespace_entries_iter(client->db->cluster, name, name_len);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_len =
            cluster_namespace_len(client->db->cluster, name, name_len);

        builder_add_arr(builder, cluster_len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            Object* obj = ((Object*)(cur->value));
            if (obj->type == OINT64) {
                builder_add_int(builder, obj->data.i64);
            } else if (obj->type == STRING) {
                size_t len = vstr_len(obj->data.str);
                builder_add_string(builder, obj->data.str, len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;

    case CLUSTER_ENTRIES: {
        HtEntriesIter* iter;
        ClusterEntriesCmd cluster_entries_cmd;
        Entry* cur;
        uint8_t* name;
        size_t name_len, cluster_len;

        if (client->db->cluster->len == 0) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_entries_cmd = cmd->expression.cluster_entries;

        name = cluster_entries_cmd.cluster_name.value;
        name_len = cluster_entries_cmd.cluster_name.len;

        iter =
            cluster_namespace_entries_iter(client->db->cluster, name, name_len);
        if (iter == NULL) {
            builder_add_none(builder);
            conn->write_buf = builder_out(builder);
            conn->write_size = builder->ins;
            return;
        }

        cluster_len =
            cluster_namespace_len(client->db->cluster, name, name_len);

        builder_add_arr(builder, cluster_len);

        for (cur = iter->cur; cur != NULL;
             ht_entries_next(iter), cur = iter->cur) {
            uint8_t* key = cur->key;
            size_t key_len = cur->key_len;
            Object* obj = ((Object*)cur->value);
            builder_add_arr(builder, 2);
            builder_add_string(builder, ((char*)key), key_len);
            if (obj->type == OINT64) {
                builder_add_int(builder, obj->data.i64);
            } else if (obj->type == STRING) {
                size_t len = vstr_len(obj->data.str);
                builder_add_string(builder, obj->data.str, len);
            }
        }

        ht_entries_iter_free(iter);

        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;
    case MULTI_CMD: {
        MultiCmd multi = cmd->expression.multi;
        size_t i, num_cmds = multi.len;
        builder_add_arr(builder, num_cmds);
        for (i = 0; i < num_cmds; ++i) {
            Cmd c = multi.commands[i];
            evaluate_cmd(&c, client, loglevel, builder);
        }
        free(multi.commands);
        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
    } break;
    case REPLICATE:
        replicate(client->db, client);
        builder_add_ok(builder);
        conn->write_buf = builder_out(builder);
        conn->write_size = builder->ins;
        break;
    }
}

void evaluate_message(uint8_t* data, size_t len, Client* client,
                      LogLevel loglevel) {
    Lexer l;
    Parser p;
    CmdIR cir;
    Cmd cmd;
    // Builder b;
    Connection* conn = client->conn;

    if (loglevel >= LL_VERBOSE) {
        slowlog(data, len);
    }

    l = lexer_new(data, len);
    p = parser_new(&l);
    cir = parse_cmd(&p);
    cmd = cmd_from_statement(&(cir.stmt));

    if (parser_errors_len(&p)) {
        cmdir_free(&cir);
        parser_free_errors(&p);
        builder_add_err(&(client->builder), (uint8_t*)"INVALID", 7);
        printf("invalid cmd\n");
        // conn->write_buf = calloc(sizeof(uint8_t), 10);
        // if (conn->write_buf == NULL) {
        //     fmt_error("out of memory\n");
        //     return;
        // }
        // memcpy(conn->write_buf, "-INVALID\r\n", 10);
        conn->write_buf = builder_out(&(client->builder));
        conn->write_size = client->builder.ins;
        return;
    }

    evaluate_cmd(&cmd, client, loglevel, &(client->builder));

    cmdir_free(&cir);
    parser_free_errors(&p);
}

int find_client_in_vec(void* fdp, void* c) {
    int fd = *((int*)fdp);
    Client* client = *((Client**)c);
    return fd - client->fd;
}

int find_master_in_vec(void* cmp, void* c) {
    Client* client = *((Client**)c);
    UNUSED(cmp);
    if (client->ismaster) {
        return 0;
    } else {
        return 1;
    }
}

void write_to_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_sent;
    Client* client;
    Connection* conn;
    Server* s;
    int found;

    s = client_data;

    found = vec_find(s->clients, &fd, find_client_in_vec, &client);
    if (found == -1) {
        LOG(LOG_ERROR "could not find client %d\n", fd);
        de_del_event(de, fd, flags);
        return;
    }

    conn = client->conn;
    if (conn->flags == 1) {
        // we were expecting more data but we now have it all
        evaluate_message(conn->read_buf, conn->read_pos, client, s->loglevel);
        conn->flags = 0;
    }
    if (conn->write_buf != NULL) {
        size_t write_size = conn->write_size;
        bytes_sent = write(fd, conn->write_buf, write_size);
        if (bytes_sent < 0) {
            fmt_error("failed to write to client\n");
            return;
        }

        if (bytes_sent < write_size) {
            fmt_error("failed to send all bytes\n");
            return;
        }
        builder_reset(&(client->builder));
    } else {
        bytes_sent = write(fd, "+noop\r\n", 7);
        if (bytes_sent < 0) {
            fmt_error("failed to write to client\n");
            return;
        }
        if (bytes_sent < 7) {
            fmt_error("failed to send all bytes\n");
            return;
        }
    }

    conn->flags = 0;
    de_del_event(de, fd, DE_WRITE);
    UNUSED(flags);
}

void read_from_client(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_read;
    Server* s;
    Client* client = NULL;
    Connection* conn;
    int found;

    s = client_data;

    found = vec_find(s->clients, &fd, find_client_in_vec, &client);
    if (found == -1) {
        LOG(LOG_ERROR "could not find client %d\n", fd);
        de_del_event(de, fd, flags);
        return;
    }

    conn = client->conn;

    if (conn->flags == 0) {
        memset(conn->read_buf, 0, conn->read_cap);
        conn->read_pos = 0;
    }

    bytes_read = read(fd, conn->read_buf + conn->read_pos, MAX_READ);

    if (bytes_read < 0) {
        fmt_error("closing client fd %d\n", fd);
        client_close(de, s, client, fd, flags);
        return;
    }

    if (bytes_read == 0) {
        client_close(de, s, client, fd, flags);
        return;
    }

    // we have filled the buffer
    if (bytes_read == 4096) {
        conn->flags = 1;
        conn->read_pos += MAX_READ;
        if ((conn->read_cap - conn->read_pos) < 4096) {
            conn->read_cap += MAX_READ;
            conn->read_buf =
                realloc(conn->read_buf, sizeof(uint8_t) * conn->read_cap);
            memset(conn->read_buf + conn->read_pos, 0, MAX_READ);
        }
        // if we do not read again before the write event, the message
        // will be evaluated in write_to_client before
        // writing a response
        de_add_event(de, fd, DE_WRITE, write_to_client, s);
        return;
    }

    conn->read_pos += bytes_read;

    evaluate_message(conn->read_buf, conn->read_pos, client, s->loglevel);
    conn->flags = 0;

    de_add_event(de, fd, DE_WRITE, write_to_client, s);
}

void server_accept(De* de, int fd, void* client_data, uint32_t flags) {
    Server* s;
    Connection* c;
    Client* client;
    int cfd;
    struct sockaddr_in addr = {0};
    socklen_t len = 0;

    s = ((Server*)(client_data));
    if (s->sfd != fd) {
        fmt_error("server_accept called for non sfd event\n");
        return;
    }

    cfd = tcp_accept(fd, &addr, &len);
    if (cfd == -1) {
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            fmt_error("accept blocked\n");
            return;
        } else {
            fmt_error("accept failed\n");
            return;
        }
    }

    if (make_socket_nonblocking(cfd) < 0) {
        fmt_error("failed to make fd %d (addr: %u),  nonblocking\n", cfd,
                  addr.sin_addr.s_addr);
        return;
    }

    if (s->loglevel >= LL_INFO) {
        LOG(LOG_CONNECTION "fd: %d addr: %u port: %u\n", cfd,
            addr.sin_addr.s_addr, addr.sin_port);
    }

    c = connection_create(addr.sin_addr.s_addr, addr.sin_port);
    client = client_create(c, s->db, cfd);
    vec_push(&(s->clients), &client);
    de_add_event(de, cfd, DE_READ, read_from_client, s);
}

int create_master_connection(Server* s, const char* addr, uint16_t port) {
    int res;
    int socket;
    uint32_t adder;
    Connection* c;
    Client* client;
    adder = parse_addr(addr, strlen(addr));
    /* make it blocking to start, we will make it non blocking after we have
     * connected to ensure a connection has been established
     */
    socket = create_tcp_socket(0);
    if (connect_tcp_sock_u32(socket, adder, port) == -1) {
        SLOG_ERROR;
        return -1;
    }
    res = make_socket_nonblocking(socket);
    if (res == -1) {
        SLOG_ERROR;
        close(socket);
        return -1;
    }
    c = connection_create(adder, port);
    if (c == NULL) {
        fmt_error("out of memory, buy more ram\n");
        close(socket);
        return -1;
    }
    client = client_create(c, s->db, socket);
    if (client == NULL) {
        fmt_error("out of memory, buy more ram\n");
        free(c);
        close(socket);
    }
    client->ismaster = 1;

    builder_add_string(&(client->builder), "REPLICATE", 9);
    client->conn->write_buf = builder_out(&(client->builder));
    client->conn->write_size = client->builder.ins;
    vec_push(&(s->clients), &client);
    return 0;
}

void read_from_master(De* de, int fd, void* client_data, uint32_t flags) {
    ssize_t bytes_read;
    Server* s;
    Client* client = NULL;
    Connection* conn;
    int found;

    s = client_data;

    found = vec_find(s->clients, &fd, find_client_in_vec, &client);
    if (found == -1) {
        LOG(LOG_ERROR "could not find client %d\n", fd);
        de_del_event(de, fd, flags);
        return;
    }

    conn = client->conn;

    if (conn->flags == 0) {
        memset(conn->read_buf, 0, conn->read_cap);
        conn->read_pos = 0;
    }

    bytes_read = read(fd, conn->read_buf + conn->read_pos, MAX_READ);

    if (bytes_read < 0) {
        fmt_error("closing client fd %d\n", fd);
        client_close(de, s, client, fd, flags);
        return;
    }

    if (bytes_read == 0) {
        client_close(de, s, client, fd, flags);
        return;
    }

    // we have filled the buffer
    if (bytes_read == 4096) {
        conn->flags = 1;
        conn->read_pos += MAX_READ;
        if ((conn->read_cap - conn->read_pos) < 4096) {
            conn->read_cap += MAX_READ;
            conn->read_buf =
                realloc(conn->read_buf, sizeof(uint8_t) * conn->read_cap);
            memset(conn->read_buf + conn->read_pos, 0, MAX_READ);
        }
        return;
    }

    conn->read_pos += bytes_read;

    evaluate_message(conn->read_buf, conn->read_pos, client, s->loglevel);
    conn->flags = 0;
}

int server(char* addr_str, uint16_t port, LogLevel loglevel, int isreplica) {
    Server* server;
    De* de;
    int sfd;
    uint32_t addr;
    uint8_t add_event_res;

    create_sigint_handler();

    addr = parse_addr(addr_str, strlen(addr_str));
    sfd = create_tcp_socket(YES_NONBLOCK);
    if (sfd < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (set_reuse_addr(sfd) < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (bind_tcp_sock(sfd, addr, port) < 0) {
        SLOG_ERROR;
        return -1;
    }

    if (tcp_listen(sfd, BACKLOG) < 0) {
        SLOG_ERROR;
        return -1;
    }

    server = server_create(sfd, loglevel, isreplica);
    if (server == NULL) {
        fmt_error("failed to allocate memory for server\n");
        close(sfd);
        return -1;
    }

    de = de_create(BACKLOG);
    if (de == NULL) {
        fmt_error("failed to allocate memory for event loop\n");
        return -1;
    }

    if (server->isslave == 1) {
        Client* master_client;
        uint16_t replica_port = isreplica;
        int cmc_res, found;
        cmc_res = create_master_connection(server, "127.0.0.1", replica_port);
        if (cmc_res == -1) {
            server_destroy(server);
            de_free(de);
            fmt_error("failed to connect to master\n");
            return -1;
        }

        found = vec_find(server->clients, NULL, find_master_in_vec, &master_client);
        if (found == -1) {
            server_destroy(server);
            de_free(de);
            LOG(LOG_ERROR "could not find master in client vector\n");
            return -1;
        }

        add_event_res = de_add_event(de, master_client->fd, DE_WRITE, write_to_client, server);
        if ((add_event_res == 1) || (add_event_res == 2)) {
            de_free(de);
            server_destroy(server);
            return -1;
        }

        add_event_res = de_add_event(de, master_client->fd, DE_READ, read_from_master, server);
        if ((add_event_res == 1) || (add_event_res == 2)) {
            de_free(de);
            server_destroy(server);
            return -1;
        }
    }

    add_event_res = de_add_event(de, sfd, DE_READ, server_accept, server);

    if ((add_event_res == 1) || (add_event_res == 2)) {
        free(de);
        server_destroy(server);
        return -1;
    }

    if (server->loglevel >= LL_INFO) {
        LOG(LOG_INFO "server listening on %s:%u\n", addr_str, port);
    }

    de_await(de);

    if (server->loglevel >= LL_INFO) {
        LOG(LOG_INFO "server shutting down\n");
    }

    server_destroy(server);
    de_free(de);

    return 0;
}
