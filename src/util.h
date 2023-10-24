#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>
#include <stddef.h>

#define SLOG_ERROR fprintf(stderr, "%s:%d error %d %s\n", __FILE__, __LINE__, errno, strerror(errno));

#define fmt_error(...) \
    fprintf(stderr, "%s:%d ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); \
    fflush(stderr);

#define UNUSED(v) ((void)v)

void get_random_bytes(uint8_t *p, size_t len);
int create_sigint_handler(void);

#endif
