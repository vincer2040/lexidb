#ifndef _DE_H_
#define _DE_H_

#include <stdint.h>

#define DE_OK       0
#define DE_ERR      1

#define DE_NONE     0
#define DE_READ     1
#define DE_WRITE    2
#define DE_BOTH     4

struct De;

typedef void DeFileFn(struct De* de, int fd, void* client_data, uint32_t flags);

typedef struct{
    int epollfd;
    struct epoll_event* events;
}DeApi;

typedef struct{
    uint32_t flags;
    DeFileFn* read_fn;
    DeFileFn* write_fn;
    void* client_data;
}DeFileEvent;

typedef struct{
    int fd;
    uint32_t flags;
}DeFired;

typedef struct De{
    int maxfd;
    int nnem;
    DeFileEvent* events;
    DeFired* fired;
    DeApi* api;
}De;

extern De* de_create(int nnem);
extern int de_await(De* de);
extern uint8_t de_add_event(De* de, int fd, uint32_t flags, DeFileFn* fn, void* client_data);
extern uint8_t de_set_nnem(De* de, int nnem);
extern int de_del_event(De* de, int fd, uint32_t flags);
extern void de_free(De* de);

#endif
