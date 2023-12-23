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
    ev_api* state = calloc(1, sizeof(ev_api));

    if (!state) {
        return -1;
    }
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    ev->api = state;
    return 0;
}

static int ev_api_resize(ev* ev, int setsize) {
    ev_unused(ev);
    /* Just ensure we have enough room in the fd_set type. */
    if (setsize >= FD_SETSIZE)
        return -1;
    return 0;
}

static void ev_api_free(ev* ev) { free(ev->api); }

static int ev_api_add_event(ev* ev, int fd, int mask) {
    ev_api* state = ev->api;

    if (mask & EV_READ) {
        FD_SET(fd, &state->rfds);
    }
    if (mask & EV_WRITE) {
        FD_SET(fd, &state->wfds);
    }
    return 0;
}

static void ev_api_del_event(ev* ev, int fd, int mask) {
    ev_api* state = ev->api;
    if (mask & EV_READ) {
        FD_CLR(fd, &state->rfds);
    }
    if (mask & EV_WRITE) {
        FD_CLR(fd, &state->wfds);
    }
}

static int ev_api_poll(ev* ev, struct timeval* tvp) {
    ev_api* state = ev->api;
    int retval, j, numevents = 0;

    memcpy(&state->_rfds, &state->rfds, sizeof(fd_set));
    memcpy(&state->_wfds, &state->wfds, sizeof(fd_set));

    retval = select(ev->max_fd + 1, &state->_rfds, &state->_wfds, NULL, tvp);
    if (retval > 0) {
        for (j = 0; j <= ev->max_fd; j++) {
            int mask = 0;
            ev_file_event* fe = &ev->events[j];

            if (fe->mask == EV_NONE) {
                continue;
            }
            if (fe->mask & EV_READ && FD_ISSET(j, &state->_rfds)) {
                mask |= EV_READ;
            }
            if (fe->mask & EV_WRITE && FD_ISSET(j, &state->_wfds)) {
                mask |= EV_WRITE;
            }
            ev->fired[numevents].fd = j;
            ev->fired[numevents].mask = mask;
            numevents++;
        }
    } else if (retval == -1 && errno != EINTR) {
        fprintf(stderr, "aeApiPoll: select, %s", strerror(errno));
    }

    return numevents;
}

static const char* api_name(void) { return "select"; }
