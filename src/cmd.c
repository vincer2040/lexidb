#include "cmd.h"
#include "auth.h"
#include "object.h"
#include "server.h"
#include "util.h"

int ping_cmd(client* client, const cmd* cmd) {
    UNUSED(cmd);
    client_add_reply_pong(client);
    return 0;
}

int set_cmd(client* client, const cmd* cmd) {
    object key_obj = cmd->data.set.key;
    object value = cmd->data.set.value;
    vstr key;
    int res;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    }

    key = key_obj.data.string;

    res = db_insert_key(client->db, &key, &value);
    if (res != 0) {
        vstr err = vstr_from("out of memory. shutting down\n");
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        abort();
    }
    client_add_reply_ok(client);
    return 0;
}

int get_cmd(client* client, const cmd* cmd) {
    object key_obj = cmd->data.get;
    vstr key;
    const object* value;
    object string_obj;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    }

    key = key_obj.data.string;

    value = db_get_key(client->db, &key);

    if (value == NULL) {
        client_add_reply_null(client);
        return 0;
    }

    string_obj = object_to_string(value);

    client_add_reply_object(client, &string_obj);

    object_free(&string_obj);
    return 0;
}

int del_cmd(client* client, const cmd* cmd) {
    object key_obj = cmd->data.get;
    vstr key;
    int res;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    }

    key = key_obj.data.string;

    res = db_delete_key(client->db, &key);

    if (res == 0) {
        client_add_reply_zero(client);
        return 0;
    }

    client_add_reply_one(client);
    return 0;
}

int auth_cmd(client* client, const cmd* cmd) {
    vstr username = cmd->data.auth.username;
    vstr password = cmd->data.auth.password;
    user* user = authenicate_user(&username, &password);
    if (user == NULL) {
        client_add_reply_invalid_auth(client);
        return 0;
    }
    client_add_reply_ok(client);
    client->authenticated = 1;
    return 0;
}

int keys_cmd(client* client, const cmd* cmd) {
    // uint64_t num_keys = db_get_num_keys(client->db);
    // vmap_iter iter = vmap_iter_new(client->db->keys);
    // client_add_reply_array(client, num_keys);
    // while (iter.cur) {
    //     const vmap_entry* cur = iter.cur;
    //     const vstr* key = vmap_entry_get_key(cur);
    //     client_add_reply_bulk_string(client, key);
    //     vmap_iter_next(&iter);
    // }
    return 0;
}

int type_cmd(client* client, const cmd* cmd) {
    object key_obj = cmd->data.type;
    vstr key;
    const object* value;
    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s, want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    }
    key = key_obj.data.string;
    value = db_get_key(client->db, &key);
    if (value == NULL) {
        client_add_reply_none(client);
        return 0;
    }
    switch (value->type) {
    case OT_Null:
        client_add_reply_none(client);
        break;
    case OT_Int:
        client_add_reply_simple_string(client, "integer", 7);
        break;
    case OT_Double:
        client_add_reply_simple_string(client, "double", 6);
        break;
    case OT_Boolean:
        client_add_reply_simple_string(client, "bool", 4);
        break;
    case OT_String:
        client_add_reply_simple_string(client, "string", 6);
        break;
    default:
        unreachable();
    }
    return 0;
}

int incr_cmd(client* client, const cmd* cmd) {
    object key_obj = cmd->data.incr;
    vstr key;
    const object* cur_value;
    object new_obj = {0};
    int64_t new_value;
    int res;
    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s, want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    }
    key = key_obj.data.string;
    cur_value = db_get_key(client->db, &key);
    if (cur_value == NULL) {
        new_value = 1;
        new_obj.type = OT_Int;
        new_obj.data.integer = new_value;
        res = db_insert_key(client->db, &key, &new_obj);
    } else if (cur_value->type != OT_Int) {
        vstr err = vstr_format("invalid type: %s, want integer",
                               object_type_to_string(cur_value));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return -1;
    } else {
        new_value = cur_value->data.integer + 1;
        new_obj.type = OT_Int;
        new_obj.data.integer = new_value;
        res = db_insert_key(client->db, &key, &new_obj);
    }
    if (res != 0) {
        vstr err = vstr_from("out of memory. shutting down\n");
        server_log(LL_Info, vstr_data(&err), vstr_len(&err));
        vstr_free(&err);
        abort();
    }
    client_add_reply_integer(client, new_value);
    return 0;
}

void cmd_free(cmd* cmd) {
    switch (cmd->type) {
    case CT_Get:
        object_free(&cmd->data.get);
        break;
    case CT_Del:
        object_free(&cmd->data.del);
        break;
    case CT_Auth:
        vstr_free(&cmd->data.auth.username);
        vstr_free(&cmd->data.auth.password);
        break;
    case CT_Type:
        object_free(&cmd->data.type);
        break;
    default:
        break;
    }
}

void cmd_free_full(cmd* cmd) {
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
    case CT_Auth:
        vstr_free(&cmd->data.auth.username);
        vstr_free(&cmd->data.auth.password);
        break;
    case CT_Type:
        object_free(&cmd->data.type);
        break;
    case CT_Incr:
        object_free(&cmd->data.incr);
        break;
    default:
        break;
    }
}
