#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <errno.h>

#include "de.h"

static uint8_t api_create(De* de)
{
    DeApi* api;

    api = calloc(1, sizeof *api);
    if (api == NULL)
    {
        return 1;
    }
    api->events = calloc(de->nnem, sizeof *(api->events));
    if (api->events == NULL)
    {
        free(api);
        return 1;
    }
    api->epollfd = epoll_create1(0);
    if (api->epollfd == -1)
    {
        free(api->events);
        free(api);
        return 1;
    }
    de->api = api;
    return 0;
}

static uint8_t api_resize(De* de, int nnem)
{
    DeApi* api;
    api = de->api;
    api = realloc(api, (sizeof *api) * nnem);
    if (api == NULL)
    {
        return 1;
    }
    return 0;
}

static uint8_t api_add_event(De* de, int fd, uint32_t flags)
{
    DeApi* api;
    struct epoll_event ee = { 0 };
    int opt;
    api = de->api;
    opt = de->events[fd].flags == DE_NONE ?
        EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    flags |= de->events[fd].flags;
    if (flags & DE_READ) ee.events |= EPOLLIN;
    if (flags & DE_WRITE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (epoll_ctl(api->epollfd, opt, fd, &ee) == -1)
    {
        fprintf(stderr, "%d %s\n", errno, strerror(errno));
        return 1;
    }
    return 0;
}

static uint8_t api_del_event(De* de, int fd, uint32_t delflags)
{
    DeApi* api;
    uint32_t flags;
    struct epoll_event ee = { 0 };
    api = de->api;
    flags = de->events[fd].flags & (~delflags);
    ee.events = 0;
    if (flags & DE_READ) ee.events |= EPOLLIN;
    if (flags & DE_WRITE) ee.events |= EPOLLOUT;
    ee.data.fd = fd;
    if (flags != DE_NONE)
    {
        if (epoll_ctl(api->epollfd, EPOLL_CTL_MOD, fd, &ee) == -1)
        {
            return 1;
        }
        return 0;
    }
    if (epoll_ctl(api->epollfd, EPOLL_CTL_DEL, fd, &ee) == -1)
    {
        fprintf(stderr, "%d %s\n", errno, strerror(errno));
        return 1;
    }
    return 0;
}

static int api_await(De* de)
{
    DeApi* api;
    int num, i;
    api = de->api;
    num = epoll_wait(api->epollfd, api->events, de->nnem, -1);
    if ((num == -1) && (errno != EINTR)) /* not interrupted by signal handler */
    {
        abort();
    }
    for (i = 0; i < num; i++)
    {
        struct epoll_event* e;
        uint32_t flags = 0;
        e = ((api->events) + i);
        if (e->events & EPOLLIN)    flags |= DE_READ;
        if (e->events & EPOLLOUT)   flags |= DE_WRITE;
        if (e->events & EPOLLERR)   flags |= (DE_READ|DE_WRITE);
        if (e->events & EPOLLHUP)   flags |= (DE_READ|DE_WRITE);
        de->fired[i].fd = e->data.fd;
        de->fired[i].flags = flags;
    }
    return num;
}

static int fire_events(De* de)
{
    int num, i;
    num = api_await(de);
    for (i = 0; i < num; i++)
    {
        int fd, flags, fired, invert;
        DeFileEvent* dfe;
        fd = de->fired[i].fd;
        flags = de->fired[i].flags;
        dfe = &(de->events[fd]);
        invert = dfe->flags & DE_BOTH;
        fired = 0;
        if (!invert && (dfe->flags & flags & DE_READ))
        {
            dfe->read_fn(de, fd, dfe->client_data, flags);
            fired++;
        }
        if (dfe->flags & flags & DE_WRITE)
        {
            if ((!fired) || (dfe->write_fn != dfe->read_fn))
            {
                dfe->write_fn(de, fd, dfe->client_data, flags);
                fired++;
            }
        }
        if (invert)
        {
            if ((dfe->flags & flags & DE_READ) && (!fired || (dfe->write_fn != dfe->read_fn)))
            {
                dfe->read_fn(de, fd, dfe->client_data, flags);
                fired++;
            }
        }
    }
    return num;
}

uint8_t de_set_nnem(De* de, int nnem)
{
    int i;
    if (nnem == de->nnem) return 0;
    if (nnem >= de->maxfd) return 1;
    if (api_resize(de, nnem)) return 1;
    de->events = realloc(de->events, (sizeof *(de->events)) * nnem);
    if (de->events == NULL)
    {
        return 1;
    }
    de->fired = realloc(de->fired, (sizeof *(de->events)) * nnem);
    if (de->fired == NULL)
    {
        return 1;
    }
    for (i = de->maxfd + 1; i < nnem; i++)
    {
        de->events[i].flags = DE_NONE;
        de->events[i].read_fn = NULL;
        de->events[i].write_fn = NULL;
        de->events[i].client_data = NULL;
    }
    return 0;
}

uint8_t de_add_event
(
        De* de,
        int fd,
        uint32_t flags,
        DeFileFn* fn,
        void* client_data
)
{
    DeFileEvent* dfe;
    if (fd >= de->nnem)
    {
        return 1;
    }
    dfe = &(de->events[fd]);
    if (api_add_event(de, fd, flags))
    {
        return 2;
    }
    dfe->flags |= flags;
    if (flags & DE_READ) dfe->read_fn = fn;
    if (flags & DE_WRITE) dfe->write_fn = fn;
    dfe->client_data = client_data;
    if (fd > de->maxfd) de->maxfd  = fd;
    return 0;
}

int de_del_event(De* de, int fd, uint32_t flags)
{
    DeFileEvent* fe = &(de->events[fd]);
    if (fe->flags & DE_NONE) return 0;
    if (flags & DE_WRITE) flags |= DE_BOTH;
    if (api_del_event(de, fd, flags)) return -1;
    fe->flags = fe->flags & (~flags);
    return 0;
}

int de_await(De* de)
{
    for (;;)
    {
        if (fire_events(de) == -1)
        {
            break;
        }
    }
    return 0;
}

De* de_create(int nnem)
{
    De* de;
    de = calloc(1, sizeof *de);
    if (de == NULL) return NULL;
    de->maxfd = -1;
    de->nnem = nnem;
    de->events = calloc(nnem, sizeof *(de->events));
    if (de->events == NULL)
    {
        free(de);
        return NULL;
    }
    de->fired = calloc(nnem, sizeof *(de->fired));
    if (de->fired == NULL)
    {
        free(de->events);
        free(de);
        return NULL;
    }
    api_create(de);
    return de;
}

static void de_api_free(DeApi* api)
{
    free(api->events);
    free(api);
}

void de_free(De* de)
{
    de_api_free(de->api);
    free(de->events);
    free(de->fired);
    free(de);
}

