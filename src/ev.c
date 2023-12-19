#include "ev.h"
#include <stdlib.h>

#include "ev_select.c"

ev* ev_new(int num_fds) {
    ev* ev;

    ev = calloc(1, sizeof *ev);
    if (ev == NULL) {
        return NULL;
    }

    ev->fired = calloc(num_fds, sizeof(ev_fired_event));
    if (ev->fired == NULL) {
        free(ev);
        return NULL;
    }

    ev->events = calloc(num_fds, sizeof(ev_file_event));
    if (ev->events == NULL) {
        free(ev->fired);
        free(ev);
        return NULL;
    }

    if (ev_api_new(ev) == -1) {
        free(ev->fired);
        free(ev->events);
        free(ev);
        return NULL;
    }

    ev->num_fds = num_fds;
    ev->max_fd = -1;
    return ev;
}

int ev_get_num_fds(ev* ev) { return ev->num_fds; }

int ev_resize_num_fds(ev* ev, int num_fds) {
    void* tmp;
    int i;
    if (ev->num_fds == num_fds) {
        return 0;
    }

    if (ev->max_fd >= num_fds) {
        return -1;
    }

    if (ev_api_resize(ev, num_fds) == -1) {
        return -1;
    }

    tmp = realloc(ev->events, (sizeof(ev_file_event) * num_fds));
    if (tmp == NULL) {
        return -1;
    }
    ev->events = tmp;

    tmp = realloc(ev->fired, (sizeof(ev_fired_event) * num_fds));
    if (tmp == NULL) {
        return -1;
    }
    ev->fired = tmp;

    for (i = 0; i < num_fds; ++i) {
        ev->events[i].mask = EV_NONE;
    }

    return 0;
}

int ev_add_event(ev* ev, int fd, int mask, ev_file_fn* fn, void* client_data) {
    ev_file_event* e;
    if (fd >= ev->num_fds) {
        return -1;
    }

    e = &(ev->events[fd]);

    if (ev_api_add_event(ev, fd, mask) == -1) {
        return -1;
    }

    e->mask |= mask;

    if (mask & EV_READ) {
        e->read_fn = fn;
    }
    if (mask & EV_WRITE) {
        e->write_fn = fn;
    }

    e->client_data = client_data;

    if (fd > ev->max_fd) {
        ev->max_fd = fd;
    }

    return 0;
}

void ev_delete_event(ev* ev, int fd, int mask) {
    ev_file_event* e;
    if (fd >= ev->num_fds) {
        return;
    }

    e = &(ev->events[fd]);

    if (e->mask == EV_NONE) {
        return;
    }

    if (mask & EV_WRITE) {
        mask |= EV_BOTH;
    }

    e->mask = e->mask & (~mask);

    if (fd == ev->max_fd && e->mask == EV_NONE) {
        int i;
        for (i = ev->max_fd - 1; i >= 0; i--) {
            if (ev->events[i].mask != EV_NONE) {
                break;
            }
        }
        ev->max_fd = i;
    }

    ev_api_del_event(ev, fd, mask);
}

static int ev_process_events(ev* ev) {
    int num_events, processed = 0;
    struct timeval tv = {0};
    int i;

    if (ev->max_fd == -1) {
        return processed;
    }
    tv.tv_sec = tv.tv_usec = 0;
    num_events = ev_api_poll(ev, &tv);
    for (i = 0; i < num_events; ++i) {
        int fd = ev->fired[i].fd;
        int mask = ev->fired[i].mask;
        ev_file_event* e = &(ev->events[i]);
        int fired = 0;
        int invert = e->mask & EV_BOTH;

        if (!invert && e->mask & mask & EV_READ) {
            e->read_fn(ev, fd, e->client_data, mask);
            fired++;
            e = &(ev->events[fd]);
        }

        if (e->mask & mask & EV_WRITE) {
            if (!fired || e->write_fn != e->read_fn) {
                e->write_fn(ev, fd, e->client_data, mask);
                fired++;
            }
        }

        if (invert) {
            e = &(ev->events[fd]);
            if ((e->mask & mask & EV_READ) &&
                (!fired || e->write_fn != e->read_fn)) {
                e->read_fn(ev, fd, e->client_data, mask);
                fired++;
            }
        }

        processed++;
    }
    return processed;
}

void ev_await(ev* ev) {
    for (;;) {
        ev_process_events(ev);
    }
}

void ev_free(ev* ev) {
    ev_api_free(ev);
    free(ev->fired);
    free(ev->events);
    free(ev);
}

const char* ev_api_name(void) { return api_name(); }
