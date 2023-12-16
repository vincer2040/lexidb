#include "ev.h"
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

typedef struct ev_api {
    fd_set rfds;
    fd_set wfds;

    fd_set _rfds;
    fd_set _wfds;
} ev_api;

static int ev_api_new(ev* ev) {
    ev_api* api = calloc(1, sizeof *api);
    if (api == NULL) {
        return -1;
    }
    FD_ZERO(&(api->rfds));
    FD_ZERO(&(api->wfds));
    ev->api = api;
    return 0;
}

static int ev_api_resize(ev* ev, int num_fds) {
    ev_unused(ev);
    if (num_fds >= FD_SETSIZE) {
        return -1;
    }
    return 0;
}

static void ev_api_free(ev* ev) { free(ev->api); }

static int ev_api_add_event(ev* ev, int fd, int mask) {
    ev_api* api = ev->api;
    if (mask & EV_READ) {
        FD_SET(fd, &(api->rfds));
    }
    if (mask & EV_WRITE) {
        FD_SET(fd, &(api->wfds));
    }
    return 0;
}

static void ev_api_del_event(ev* ev, int fd, int mask) {
    ev_api* api = ev->api;
    if (mask & EV_READ) {
        FD_CLR(fd, &(api->rfds));
    }
    if (mask & EV_WRITE) {
        FD_CLR(fd, &(api->wfds));
    }
}

static int ev_api_poll(ev* ev, struct timeval* tv) {
    ev_api* api = ev->api;
    int res, i, num_events = 0;

    memcpy(&(api->_rfds), &(api->rfds), sizeof(fd_set));
    memcpy(&(api->_wfds), &(api->wfds), sizeof(fd_set));

    res = select(ev->max_fd + 1, &(api->_rfds), &(api->_wfds), NULL, tv);
    if (res > 0) {
        for (i = 0; i <= ev->max_fd; ++i) {
            int mask = 0;
            ev_file_event* e = &(ev->events[i]);
            if (e->mask == EV_NONE) {
                continue;
            }

            if ((e->mask & EV_READ) && FD_ISSET(i, &(api->_rfds))) {
                mask |= EV_READ;
            }

            if ((e->mask & EV_WRITE) && FD_ISSET(i, &(api->_wfds))) {
                mask |= EV_WRITE;
            }
            ev->fired[num_events].fd = i;
            ev->fired[num_events].mask = mask;
            num_events++;
        }
    } else if ((res == -1) && (errno != EINTR)) {
        fprintf(stderr, "ev api poll: select %s\n", strerror(errno));
        return -1;
    }

    return num_events;
}

static const char* api_name(void) { return "select"; }
