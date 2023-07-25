#define _POSIX_C_SOURCE 1
#include "util.h"
#include "db.h"
#include "sha256.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// used to check if sigint has been sent to process
// used to properly shut down the server,
// wait for child processes, etc
volatile sig_atomic_t sig_int_received = 0;

void handler(int mode) {
    sig_int_received = 1;
    UNUSED(mode);
}

int create_sigint_handler(void) {
    struct sigaction sa = {0};
    sa.sa_handler = &handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        return -1;
    }
    return 0;
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

int want_version_long(char* argv, size_t len) {
    if ((len == 9) && (strncmp(argv, "--version", 9) == 0)) {
        return 0;
    }

    return -1;
}

int want_version_short(char* argv, size_t len) {
    if ((len == 2) && (strncmp(argv, "-v", 2) == 0)) {
        return 0;
    }

    return -1;
}

int want_help_long(char* argv, size_t len) {
    if ((len == 6) && (strncmp(argv, "--help", 6) == 0)) {
        return 0;
    }

    return -1;
}

int want_help_short(char* argv, size_t len) {
    if ((len == 2) && (strncmp(argv, "-h", 2) == 0)) {
        return 0;
    }

    return -1;
}

void print_version(void) { printf("version: %s\n", VERSION); }

void print_help(char* program_name) {
    printf("usage: %s [options]\n\n%s", program_name, HELP);
}

int done_from_args(int argc, char** argv) {
    int i;
    for (i = 1; i < argc; ++i) {
        char* arg = argv[i];
        if (want_version_long(arg, strlen(arg)) == 0) {
            print_version();
            goto done;
        }
        if (want_version_short(arg, strlen(arg)) == 0) {
            print_version();
            goto done;
        }
        if (want_help_long(arg, strlen(arg)) == 0) {
            print_help(argv[0]);
            goto done;
        }
        if (want_help_short(arg, strlen(arg)) == 0) {
            print_help(argv[0]);
            goto done;
        }
        if (strncmp(arg, "--logfile", 9) == 0) {
            if (i + 1 >= argc) {
                printf("--logfile requires an argument <logfile>\n");
                goto done;
            }
            ++i;
            continue;
        }
        printf("unknown argument: %s\n", argv[i]);
        printf("use %s --help to show the help screen\n", argv[0]);
        goto done;
    }
    return 0;

done:
    return 1;
}
