#include "ev.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

typedef struct ev_api {
    int epfd;
    struct epoll_event* events;
} ev_api;

static int ev_api_new(ev* ev) {
    ev_api* state = calloc(1, sizeof(ev_api));
    if (state == NULL) {
        return -1;
    }
    state->events = calloc(ev->num_fds, sizeof(ev_api));
    if (state->events == NULL) {
        free(state);
        return -1;
    }
    state->epfd = epoll_create(1024);
    if (state->epfd == -1) {
        free(state->events);
        free(state);
        return -1;
    }
    ev->api = state;
    return 0;
}

static int ev_api_resize(ev* ev, int setsize) {
    ev_api* state = ev->api;
    void* tmp;
    tmp = realloc(state->events, (sizeof(struct epoll_event)) * setsize);
    if (tmp == NULL) {
        return -1;
    }
    state->events = tmp;
    return 0;
}

static void ev_api_free(ev* ev) {
    ev_api* state = ev->api;
    close(state->epfd);
    free(state->events);
    free(state);
}

static int ev_api_add_event(ev* ev, int fd, int mask) {
    ev_api* state = ev->api;
    struct epoll_event e = {0};
    int op = ev->events[fd].mask == EV_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    e.events = 0;
    mask |= ev->events[fd].mask;
    if (mask & EV_READ) {
        e.events |= EPOLLIN;
    }
    if (mask & EV_WRITE) {
        e.events |= EPOLLOUT;
    }
    e.data.fd = fd;
    if (epoll_ctl(state->epfd, op, fd, &e) == -1) {
        return -1;
    }
    return 0;
}

static void ev_api_del_event(ev* ev, int fd, int delmask) {
    ev_api* state = ev->api;
    struct epoll_event e = {0};
    int mask = ev->events[fd].mask & (~delmask);
    e.events = 0;
    if (mask & EV_READ) {
        e.events |= EPOLLIN;
    }
    if (mask & EV_WRITE) {
        e.events |= EPOLLOUT;
    }
    e.data.fd = fd;
    if (mask != EV_NONE) {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &e);
    } else {
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &e);
    }
}

static int ev_api_poll(ev* ev, struct timeval* tvp) {
    ev_api* state = ev->api;
    int retval, num_events = 0;
    int time = tvp ? (tvp->tv_sec * 1000 + (tvp->tv_usec + 999) / 1000) : -1;
    retval = epoll_wait(state->epfd, state->events, ev->num_fds, time);
    if (retval > 0) {
        int j;
        num_events = retval;
        for (j = 0; j < num_events; ++j) {
            int mask = 0;
            struct epoll_event* e = state->events + j;
            if (e->events & EPOLLIN) {
                mask |= EV_READ;
            }
            if (e->events & EPOLLOUT) {
                mask |= EV_WRITE;
            }
            if (e->events & EPOLLERR) {
                mask |= (EV_WRITE | EV_READ);
            }
            if (e->events & EPOLLHUP) {
                mask |= (EV_WRITE | EV_READ);
            }
            ev->fired[j].fd = e->data.fd;
            ev->fired[j].mask = mask;
        }
    } else if (retval == -1 && errno != EINTR) {
        fprintf(stderr, "epoll_wait: %s", strerror(errno));
        num_events = retval;
    }
    return num_events;
}

static const char* api_name(void) { return "epoll"; }
