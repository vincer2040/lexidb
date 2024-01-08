#include "sha256.h"
#include "util.h"
#include "vstr.h"

#define HASH_PASSWORD_LEN (SHA256_BLOCK_SIZE * 2)

int time_safe_compare(const char* a, const char* b, size_t len) {
    int res = 0;
    size_t i;
    for (i = 0; i < len; ++i) {
        res |= (a[i] ^ b[i]);
    }
    return res;
}

vstr hash_password(const char* p, size_t len) {
    SHA256_CTX ctx;
    unsigned char hash[SHA256_BLOCK_SIZE];
    char hex[HASH_PASSWORD_LEN] = {0};
    char* cset = "0123456789abcdef";
    size_t i;

    sha256_init(&ctx);
    sha256_update(&ctx, (unsigned char*)p, len);
    sha256_final(&ctx, hash);

    for (i = 0; i < SHA256_BLOCK_SIZE; ++i) {
        hex[i * 2] = cset[((hash[i] & 0xF0) >> 4)];
        hex[i * 2 + 1] = cset[(hash[i] & 0xF)];
    }

    return vstr_from_len(hex, HASH_PASSWORD_LEN);
}
