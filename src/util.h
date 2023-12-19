#ifndef __UTIL_H__

#define __UTIL_H__

#include <stddef.h>
#include <stdint.h>

typedef int cmp_fn(void* a, void* b);

typedef void free_fn(void* ptr);

void get_random_bytes(uint8_t* p, size_t len);

struct timespec get_time(void);

typedef enum {
    Ok,
    Err,
} result_type;

#define result_t(val_type, err_type)                                           \
    typedef struct {                                                           \
        result_type type;                                                      \
        union {                                                                \
            val_type ok;                                                       \
            err_type err;                                                      \
        } data;                                                                \
    } result_##val_type

#define result(val_type) result_##val_type

#endif /* __UTIL_H__ */
