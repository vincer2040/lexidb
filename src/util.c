#define _XOPEN_SOURCE 600
#include "util.h"
#include "result.h"
#include "sha256.h"
#include "vstr.h"
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t sig_int_received = 0;

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

struct timespec get_time(void) {
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return time;
}

void handler(int mode) {
    ((void)mode);
    sig_int_received = 1;
}

int create_sigint_handler(void) {
    struct sigaction sa = {0};
    sa.sa_handler = &handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        return -1;
    }
    return 0;
}

char* get_execuable_path(void) {
    char* path;
    char buf[1024];
    ssize_t res = readlink("/proc/self/exe", buf, (sizeof buf) - 1);
    if (res == -1) {
        return NULL;
    }
    path = malloc(res + 1);
    if (path == NULL) {
        return NULL;
    }
    memset(path, 0, res + 1);
    memcpy(path, buf, res);
    return path;
}

vstr get_os_name(void) {
    struct utsname n = {0};
    int r = uname(&n);
    assert(r >= 0);
    vstr s = vstr_format("%s %s %s", n.sysname, n.release, n.machine);
    return s;
}

result(vstr) read_file(const char* path) {
    result(vstr) res = {0};
    vstr s = vstr_new();
    char ch;
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        res.type = Err;
        res.data.err = vstr_format("failed to open path %s (errno: %d) %s",
                                   path, errno, strerror(errno));
        return res;
    }
    while ((ch = fgetc(f)) != EOF) {
        int push_res = vstr_push_char(&s, ch);
        if (push_res == -1) {
            vstr_free(&s);
            res.type = Err;
            res.data.err = vstr_from("failed to read full file");
            fclose(f);
            return res;
        }
    }
    fclose(f);
    res.type = Ok;
    res.data.ok = s;
    return res;
}
