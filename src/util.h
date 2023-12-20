#ifndef __UTIL_H__

#define __UTIL_H__

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

typedef int cmp_fn(void* a, void* b);

typedef void free_fn(void* ptr);

void get_random_bytes(uint8_t* p, size_t len);

struct timespec get_time(void);

int create_sigint_handler(void);

#endif /* __UTIL_H__ */
