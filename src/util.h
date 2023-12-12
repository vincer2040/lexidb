#ifndef __UTIL_H__

#define __UTIL_H__

#include <stddef.h>
#include <stdint.h>

typedef int cmp_fn(void* a, void* b);

typedef void free_fn(void* ptr);

void get_random_bytes(uint8_t* p, size_t len);

#endif /* __UTIL_H__ */
