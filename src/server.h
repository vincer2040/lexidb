#ifndef __SERVER_H__

#define __SERVER_H__

#define _XOPEN_SOURCE 600
#include "builder.h"
#include "ev.h"
#include "ht.h"
#include "queue.h"
#include "set.h"
#include "vec.h"
#include "vstr.h"
#include <stdint.h>
#include <time.h>

#define AUTHENTICATED (1 << 1)

typedef struct {
    ht dict;
    set set;
    queue queue;
    vec* vec;
} lexidb;

typedef enum {
    None = 0,
    Info = 1,
    Debug = 2,
    Verbose = 3,
} log_level;

typedef struct {
    pid_t pid;             /* the pid of the process */
    int sfd;               /* the socket file descriptor */
    log_level log_level;   /* the amount to log */
    uint16_t flags;        /* no use as of now */
    uint16_t port;         /* the port the server is listening on*/
    char* executable_path; /* the path of the executable */
    vstr os_name;          /* name of the host's operating system */
    vstr addr;             /* the host address as a vstr */
    vstr conf_file_path;   /* the path of the configuration file */
    lexidb db;             /* the database */
    ev* ev;                /* multiplexing api */
    vec* clients;          /* vector of clients connected to the server */
    vec* users;            /* vector of users */
    uint64_t cmd_executed; /* the number of commands the server has processed */
    struct timespec start_time; /* the time the server started */
} server;

typedef struct {
    vstr name; /* username */
    vstr password; /* password - hashed */
    uint32_t flags; /* flags for the user */
} user;

typedef struct {
    int fd;             /* file descriptor */
    uint32_t addr;      /* address */
    uint16_t port;      /* port */
    uint16_t flags;     /* flags */
    uint8_t* read_buf;  /* the buffer used in read() */
    size_t read_pos;    /* the position in the buffer to read to */
    size_t read_cap;    /* the allocation size of read_buf */
    uint8_t* write_buf; /* pointer to buffer to write to client */
    size_t write_size;  /* the amount to write */
    builder builder;    /* builder struct for constructing replies */
    user user;          /* the user associated with this connection */
    struct timespec time_connected; /* time this user connected */
} client;

int server_run(int argc, char* argv[]);
int time_safe_compare(const char* a, const char* b, size_t len);
vstr hash_password(const char* p, size_t len);

#endif /* __SERVER_H__ */
