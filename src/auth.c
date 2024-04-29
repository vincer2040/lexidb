#include "server.h"
#include "sha256.h"
#include "vstr.h"

#define HASH_PASSWORD_LEN (SHA256_BLOCK_SIZE * 2)

vstr hash_password(const char* p, size_t len) {
    SHA256_CTX ctx = {0};
    unsigned char hash[SHA256_BLOCK_SIZE] = {0};
    char hex[HASH_PASSWORD_LEN] = {0};
    const char* cset = "0123456789abcdef";
    size_t i;

    sha256_init(&ctx);
    sha256_update(&ctx, (const unsigned char*)p, len);
    sha256_final(&ctx, hash);

    for (i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        hex[i * 2] = cset[((hash[i] & 0xF0) >> 4)];
        hex[i * 2 + 1] = cset[(hash[i] & 0xF)];
    }

    return vstr_from_len(hex, HASH_PASSWORD_LEN);
}

int time_safe_compare(const char* a, const char* b, size_t len) {
    int res = 0;
    size_t i;
    for (i = 0; i < len; ++i) {
        res |= (a[i] ^ b[i]);
    }
    return res;
}

user* authenicate_user(const vstr* username, const vstr* password) {
    vec_iter user_iter = vec_iter_new(server.users);
    vstr hashed_vstr = hash_password(vstr_data(password), vstr_len(password));
    const char* hashed = vstr_data(&hashed_vstr);
    while (user_iter.cur) {
        const user* u = user_iter.cur;
        vec_iter password_iter;
        if (vstr_cmp(&u->username, username) != 0) {
            vec_iter_next(&user_iter);
            continue;
        }
        password_iter = vec_iter_new(u->passwords);
        while (password_iter.cur) {
            const vstr* cur_password_vstr = password_iter.cur;
            const char* cur_pass = vstr_data(cur_password_vstr);
            if (time_safe_compare(hashed, cur_pass, HASH_PASSWORD_LEN) == 0) {
                return (user*)u;
            }
            vec_iter_next(&password_iter);
        }
        vec_iter_next(&user_iter);
    }
    return NULL;
}
