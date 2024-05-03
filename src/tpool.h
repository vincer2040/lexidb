#ifndef __TPOOL_H__

#define __TPOOL_H__

#include <stddef.h>

typedef struct tpool tpool;

typedef void thread_func(const void* arg, void* output);
tpool* tpool_init(size_t num_threads);
void tpool_wait(tpool* pool);
int tpool_add_task(tpool* pool, thread_func* fn, const void* arg, void* output);
void tpool_free(tpool* pool);

#endif /* __TPOOL_H__ */

