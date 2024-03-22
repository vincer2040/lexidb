#ifndef __EV_H__

#define __EV_H__

#define EV_NONE 0
#define EV_READ 1
#define EV_WRITE 2
#define EV_BOTH 4

#define ev_unused(v) ((void)v)

struct ev;

typedef void ev_file_fn(struct ev* ev, int fd, void* client_data, int mask);

typedef struct {
    int mask;
    ev_file_fn* read_fn;
    ev_file_fn* write_fn;
    void* client_data;
} ev_file_event;

typedef struct {
    int fd;
    int mask;
} ev_fired_event;

typedef struct ev {
    int max_fd;
    int num_fds;
    ev_file_event* events;
    ev_fired_event* fired;
    void* api;
} ev;

ev* ev_new(int num_fds);
int ev_get_num_fds(ev* ev);
int ev_resize_num_fds(ev* ev, int num_fds);
int ev_add_event(ev* ev, int fd, int mask, ev_file_fn* fn, void* client_data);
void ev_delete_event(ev* ev, int fd, int mask);
void ev_await(ev* ev);
void ev_free(ev* ev);
const char* ev_api_name(void);

#endif /* __EV_H__ */
