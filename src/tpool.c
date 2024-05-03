#include "tpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct tpool_task {
    thread_func* fn;
    const void* arg;
    void* output;
    struct tpool_task* next;
} tpool_task;

struct tpool {
    tpool_task* head;
    tpool_task* tail;
    pthread_mutex_t work_mutex;
    pthread_cond_t work_cond;
    pthread_cond_t working_cond;
    size_t working_count;
    size_t num_threads;
    int stop;
};

static void* tpool_worker(void* arg);
static tpool_task* tpool_task_new(thread_func* fn, const void* arg,
                                  void* output);
static tpool_task* tpool_task_get(tpool* pool);
static void tpool_task_free(tpool_task* task);

tpool* tpool_init(size_t num_threads) {
    tpool* pool = NULL;
    size_t i;
    int rv;
    pthread_t thread;
    pool = calloc(1, sizeof *pool);
    if (pool == NULL) {
        return NULL;
    }
    rv = pthread_mutex_init(&pool->work_mutex, NULL);
    if (rv == -1) {
        goto err;
    }
    rv = pthread_cond_init(&pool->work_cond, NULL);
    if (rv == -1) {
        goto err;
    }
    rv = pthread_cond_init(&pool->working_cond, NULL);
    if (rv == -1) {
        goto err;
    }
    pool->head = pool->tail = NULL;

    for (i = 0; i < num_threads; ++i) {
        rv = pthread_create(&thread, NULL, tpool_worker, pool);
        if (rv == -1) {
            goto err;
        }
        rv = pthread_detach(thread);
        if (rv == -1) {
            goto err;
        }
    }
    pool->num_threads = num_threads;
    return pool;

err:
    free(pool);
    return NULL;
}

int tpool_add_task(tpool* pool, thread_func* fn, const void* arg,
                   void* output) {
    tpool_task* task = tpool_task_new(fn, arg, output);
    if (task == NULL) {
        return -1;
    }
    pthread_mutex_lock(&pool->work_mutex);
    if (pool->head == NULL) {
        pool->head = pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task;
    }

    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->work_mutex);
    return 0;
}

void tpool_free(tpool* pool) {
    tpool_task* task;

    pthread_mutex_lock(&pool->work_mutex);
    task = pool->head;
    while (task != NULL) {
        tpool_task* next = task->next;
        tpool_task_free(task);
        task = next;
    }
    pool->head = pool->tail = NULL;
    pool->stop = 1;
    pthread_cond_broadcast(&pool->work_cond);
    pthread_mutex_unlock(&pool->work_mutex);

    tpool_wait(pool);

    pthread_mutex_destroy(&pool->work_mutex);
    pthread_cond_destroy(&pool->work_cond);
    pthread_cond_destroy(&pool->working_cond);

    free(pool);
}

void tpool_wait(tpool* pool) {
    pthread_mutex_lock(&pool->work_mutex);
    while (1) {
        if (pool->head != NULL || (!pool->stop && pool->working_count != 0) ||
            (pool->stop && pool->num_threads != 0)) {
            pthread_cond_wait(&pool->working_cond, &pool->work_mutex);
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&pool->work_mutex);
}

static void* tpool_worker(void* arg) {
    tpool* pool = arg;
    tpool_task* task;
    while (1) {
        pthread_mutex_lock(&pool->work_mutex);
        while (pool->head == NULL && !pool->stop) {
            pthread_cond_wait(&pool->work_cond, &pool->work_mutex);
        }

        if (pool->stop) {
            pool->num_threads--;
            pthread_cond_signal(&pool->working_cond);
            pthread_mutex_unlock(&pool->work_mutex);
            return NULL;
        }

        task = tpool_task_get(pool);
        pool->working_count++;
        pthread_mutex_unlock(&pool->work_mutex);

        if (task != NULL) {
            task->fn(task->arg, task->output);
            tpool_task_free(task);
        }

        pthread_mutex_lock(&pool->work_mutex);
        pool->working_count--;
        if (!pool->stop && pool->working_count == 0 && pool->head == NULL) {
            pthread_cond_signal(&pool->working_cond);
        }
        pthread_mutex_unlock(&pool->work_mutex);
    }
    pool->num_threads--;
    pthread_cond_signal(&pool->working_cond);
    pthread_mutex_unlock(&pool->work_mutex);
    return NULL;
}

static tpool_task* tpool_task_get(tpool* pool) {
    tpool_task* task = pool->head;
    if (task == NULL) {
        return NULL;
    }
    if (task->next == NULL) {
        pool->head = pool->tail = NULL;
        return task;
    }
    pool->head = task->next;
    return task;
}

static tpool_task* tpool_task_new(thread_func* fn, const void* arg,
                                  void* output) {
    tpool_task* task = calloc(1, sizeof *task);
    task->fn = fn;
    task->arg = arg;
    task->output = output;
    task->next = NULL;
    return task;
}

static void tpool_task_free(tpool_task* task) { free(task); }
