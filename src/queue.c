#include "queue.h"
#include "util.h"
#include <memory.h>
#include <stdlib.h>

typedef struct qnode {
    struct qnode* next;
    unsigned char data[];
} qnode;

static qnode* qnode_new(size_t data_size, void* data);
static void qnode_free(qnode* node, free_fn* fn);

queue queue_new(size_t data_size) {
    queue q = {0};
    q.data_size = data_size;
    q.head = q.tail = NULL;
    return q;
}

int queue_enque(queue* q, void* data) {
    qnode* node = qnode_new(q->data_size, data);
    if (node == NULL) {
        return -1;
    }
    q->num_el++;
    if (q->num_el == 1) {
        q->head = q->tail = node;
        return 0;
    }
    q->tail->next = node;
    q->tail = node;
    return 0;
}

int queue_deque(queue* q, void* out) {
    qnode* new_head;
    if (q->num_el == 0) {
        return -1;
    }
    q->num_el--;
    if (q->num_el == 0) {
        memcpy(out, q->head->data, q->data_size);
        qnode_free(q->head, NULL);
        q->head = q->tail = NULL;
        return 0;
    }
    memcpy(out, q->head->data, q->data_size);
    new_head = q->head->next;
    qnode_free(q->head, NULL);
    q->head = new_head;
    return 0;
}

void* queue_peek(queue* q) {
    if (q->num_el == 0) {
        return NULL;
    }
    return q->head->data;
}

void queue_free(queue* q, free_fn* fn) {
    size_t i, len = q->num_el;
    qnode* node = q->head;
    for (i = 0; i < len; ++i) {
        qnode* next = node->next;
        qnode_free(node, fn);
        node = next;
    }
    q->num_el = 0;
}

static qnode* qnode_new(size_t data_size, void* data) {
    qnode* node;
    size_t needed = sizeof *node + data_size;
    node = malloc(needed);
    if (node == NULL) {
        return NULL;
    }
    memset(node, 0, needed);
    memcpy(node->data, data, data_size);
    node->next = NULL;
    return node;
}

static void qnode_free(qnode* node, free_fn* fn) {
    if (fn) {
        fn(node->data);
    }

    free(node);
}
