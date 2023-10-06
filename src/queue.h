#ifndef __QUEUE_H__

#define __QUEUE_H__

#include <stddef.h>

typedef void QFreeFn(void* ptr);

typedef struct QNode {
    struct QNode* next;
    unsigned char data[];
} QNode;

typedef struct {
    size_t len;
    size_t data_size;
    QNode* head;
    QNode* tail;
} Queue;

Queue* queue_new(size_t data_size);
int queue_peek(Queue* q, void* out);
int queue_deque(Queue* q, void* out);
int queue_enque(Queue* q, void* data);
void queue_free(Queue* q, QFreeFn* fn);

#endif
