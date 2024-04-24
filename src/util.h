#ifndef __UTIL_H__

#define __UTIL_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

char* read_file(const char* path, ssize_t* output_len);
void get_random_bytes(uint8_t* p, size_t len);
void free_vstr_in_vec(void* ptr);
char* get_executable_path(void);
char* get_real_path(const char* path);

#endif /* __UTIL_H__ */
