#include "queue.h"
#include <memory.h>
#include <stdlib.h>

Queue* queue_new(size_t data_size) {
    Queue* q;

    q = calloc(1, sizeof *q);
    if (q == NULL) {
        return NULL;
    }

    q->data_size = data_size;
    q->head = q->tail = NULL;

    return q;
}

int queue_peek(Queue* q, void* out) {
    if (q->len == 0) {
        return -1;
    }

    memcpy(out, q->head->data, q->data_size);

    return 0;
}

int queue_deque(Queue* q, void* out) {
    QNode* head;
    if (q->len == 0) {
        return -1;
    }

    q->len--;

    head = q->head;

    memcpy(out, head->data, q->data_size);

    q->head = head->next;
    free(head);

    if (q->len == 0) {
        q->head = q->tail = NULL;
    }

    return 0;
}

int queue_enque(Queue* q, void* data) {
    QNode* node;
    size_t data_size = q->data_size;
    size_t size = sizeof(QNode) + data_size;

    node = malloc(size);
    if (node == NULL) {
        return -1;
    }

    memset(node, 0, size);
    node->next = NULL;
    memcpy(node->data, data, data_size);

    if (!q->tail) {
        q->tail = q->head = node;
        q->len++;
        return 0;
    }

    q->tail->next = node;
    q->tail = node;
    q->len++;
    return 0;
}

void queue_free(Queue* q, QFreeFn* fn) {
    QNode* cur = q->head;

    while (cur != NULL) {
        QNode* at = cur;
        cur = at->next;
        if (fn) {
            fn(at->data);
        }
        free(at);
    }

    free(q);
}

QIter queue_iter_new(Queue* q) {
    QIter qi = { 0 };
    qi.q = q;
    qi.cur = q->head;
    qi.next = q->head->next;
    qi.cur_idx = 0;
    return qi;
}

void queue_iter_next(QIter* qi) {
    if (qi->next == NULL) {
        qi->cur = NULL;
        qi->next = NULL;
        return;
    }

    qi->cur = qi->next;
    qi->next = qi->cur->next;
}

