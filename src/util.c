#define _XOPEN_SOURCE 500
#include "sha256.h"
#include "vstr.h"
#include <linux/limits.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

char* read_file(const char* path, ssize_t* output_len) {
    size_t len = 0, cap = 32;
    char* res;
    FILE* file;
    char ch;
    file = fopen(path, "r");
    if (file == NULL) {
        *output_len = -1;
        return NULL;
    }
    res = calloc(cap, sizeof *res);
    if (res == NULL) {
        fclose(file);
        *output_len = -1;
        return NULL;
    }
    while ((ch = fgetc(file)) != EOF) {
        if (len >= (cap - 1)) {
            void* tmp;
            cap <<= 1;
            tmp = realloc(res, cap);
            if (tmp == NULL) {
                fclose(file);
                free(res);
                *output_len = -1;
                return NULL;
            }
            res = tmp;
            memset(res + len, 0, cap - len);
        }
        res[len] = ch;
        len++;
    }
    *output_len = len;
    return res;
}

void get_random_bytes(uint8_t* p, size_t len) {
    /* global */
    static int seed_initialized = 0;
    static uint8_t seed[64];
    static uint64_t counter = 0;

    if (!seed_initialized) {
        FILE* fp = fopen("/dev/urandom", "r");
        if (fp == NULL || fread(seed, sizeof(seed), 1, fp) != 1) {
            size_t j;
            for (j = 0; j < sizeof(seed); j++) {
                struct timeval tv;
                pid_t pid = getpid();
                gettimeofday(&tv, NULL);
                seed[j] =
                    ((unsigned char)(tv.tv_sec ^ tv.tv_usec ^ pid ^ (long)fp));
            }
        } else {
            seed_initialized = 1;
        }
        if (fp)
            fclose(fp);
    }

    while (len) {
        uint8_t digest[SHA256_BLOCK_SIZE];
        uint8_t kxor[64];
        SHA256_CTX ctx;
        size_t copylen = len > SHA256_BLOCK_SIZE ? SHA256_BLOCK_SIZE : len;
        size_t i;

        memcpy(kxor, seed, sizeof(kxor));
        for (i = 0; i < sizeof(kxor); i++)
            kxor[i] ^= 0x36;

        sha256_init(&ctx);
        sha256_update(&ctx, kxor, sizeof(kxor));
        sha256_update(&ctx, ((unsigned char*)&counter), sizeof(counter));
        sha256_final(&ctx, digest);

        memcpy(kxor, seed, sizeof(kxor));
        for (i = 0; i < sizeof(kxor); i++)
            kxor[i] ^= 0x5C;

        sha256_init(&ctx);
        sha256_update(&ctx, kxor, sizeof(kxor));
        sha256_update(&ctx, digest, SHA256_BLOCK_SIZE);
        sha256_final(&ctx, digest);

        counter++;

        memcpy(p, digest, copylen);
        len -= copylen;
        p += copylen;
    }
}

void free_vstr_in_vec(void* ptr) { vstr_free(ptr); }

char* get_executable_path(void) {
    char buf[PATH_MAX + 1] = {0};
    char* res;
    size_t len;
    ssize_t x = readlink("/proc/self/exe", buf, PATH_MAX);
    if (x == -1) {
        return NULL;
    }
    len = strlen(buf);
    res = calloc(len + 1, sizeof *res);
    if (res == NULL) {
        return NULL;
    }
    memcpy(res, buf, len);
    return res;
}

char* get_real_path(const char* path) {
    char* res;
    size_t len;
    char buf[PATH_MAX + 1] = {0};
    char* ptr = realpath(path, buf);
    if (ptr == NULL) {
        return NULL;
    }
    len = strlen(buf);
    res = calloc(len + 1, sizeof *res);
    if (res == NULL) {
        return NULL;
    }
    memcpy(res, buf, len);
    return res;
}
