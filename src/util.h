#ifndef __UTIL_H__

#define __UTIL_H__

#include "result.h"
#include "vstr.h"
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

result_t(vstr, vstr);

typedef int cmp_fn(void* a, void* b);

typedef void free_fn(void* ptr);

void get_random_bytes(uint8_t* p, size_t len);

struct timespec get_time(void);

int create_sigint_handler(void);

char* get_execuable_path(void);
const char* get_os_name(void);

result(vstr) read_file(const char* path);

#endif /* __UTIL_H__ */
