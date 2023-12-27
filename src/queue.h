#ifndef __QUEUE_H__

#define __QUEUE_H__

#include "util.h"
#include <stddef.h>

struct qnode;

typedef struct {
    size_t num_el;
    size_t data_size;
    struct qnode* head;
    struct qnode* tail;
} queue;

queue queue_new(size_t data_size);
int queue_enque(queue* q, void* data);
int queue_deque(queue* q, void* out);
void* queue_peek(queue* q);
void queue_free(queue* q, free_fn* fn);

#endif /* __QUEUE_H__ */
