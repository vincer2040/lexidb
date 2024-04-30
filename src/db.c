#include "server.h"

int db_insert_key(lexi_db* db, vstr* key, object* value) {
    return vmap_insert(&db->keys, key, value);
}

const object* db_get_key(const lexi_db* db, const vstr* key) {
    return vmap_find(db->keys, key);
}

int db_delete_key(lexi_db* db, const vstr* key) {
    return vmap_delete(&db->keys, key);
}

uint64_t db_get_num_keys(const lexi_db* db) {
    return vmap_num_keys(db->keys);
}
