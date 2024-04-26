#ifndef __UTIL_H__

#define __UTIL_H__

#include "vstr.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>

#define unreachable() do { \
    printf("%s:%d ", __FILE__, __LINE__);\
    printf("unreachable code reached\n");\
    abort();\
} while (0)

char* read_file(const char* path, ssize_t* output_len);
void get_random_bytes(uint8_t* p, size_t len);
void free_vstr_in_vec(void* ptr);
char* get_executable_path(void);
char* get_real_path(const char* path);
int create_signal_handler(void (*handler)(int), int interupt);
int vstr_has_retcar_or_newline(vstr* vs);

#endif /* __UTIL_H__ */
