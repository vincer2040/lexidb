#include "cmd.h"
#include "object.h"
#include "server.h"
#include "vmap.h"

void ping_cmd_fn(client* client, const cmd* cmd) {
    UNUSED(cmd);
    client_add_reply_pong(client);
}

void set_cmd_fn(client* client, const cmd* cmd) {
    object key_obj = cmd->data.set.key;
    object value = cmd->data.set.value;
    vstr key;
    int res;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return;
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
}

void get_cmd_fn(client* client, const cmd* cmd) {
    object key_obj = cmd->data.get;
    vstr key;
    const object* value;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return;
    }

    key = key_obj.data.string;

    value = db_get_key(client->db, &key);

    if (value == NULL) {
        client_add_reply_null(client);
        return;
    }

    client_add_reply_object(client, value);
}

void del_cmd_fn(client* client, const cmd* cmd) {
    object key_obj = cmd->data.get;
    vstr key;
    int res;

    if (key_obj.type != OT_String) {
        vstr err = vstr_format("invalid key type: %s. want: string",
                               object_type_to_string(&key_obj));
        client_add_reply_simple_error(client, &err);
        vstr_free(&err);
        return;
    }

    key = key_obj.data.string;

    res = db_delete_key(client->db, &key);

    if (res == VMAP_NO_KEY) {
        client_add_reply_zero(client);
        return;
    }

    client_add_reply_one(client);
}

void cmd_free(cmd* cmd) {
    switch (cmd->type) {
    case CT_Set:
        // key and value are moved into vmap, so we
        // do not free them.
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
