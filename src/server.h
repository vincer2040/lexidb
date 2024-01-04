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
    const char* os_name;   /* name of the host's operating system */
    vstr addr;             /* the host address as a vstr */
    vstr conf_file_path;   /* the path of the configuration file */
    lexidb db;             /* the database */
    ev* ev;                /* multiplexing api */
    vec* clients;          /* vector of clients connected to the server */
    vec* users;            /* vector of users */
    uint64_t cmd_executed; /* the number of commands the server has processed */
} server;

typedef struct {
    vstr name;
    vec* passwords;
    uint32_t flags;
} user;

typedef struct {
    int fd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_size;
    builder builder;
    user* user;
    struct timespec time_connected;
} client;

int server_run(int argc, char* argv[]);

#endif /* __SERVER_H__ */
