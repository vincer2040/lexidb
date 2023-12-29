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
    pid_t pid;
    int sfd;
    log_level log_level;
    uint16_t flags;
    uint16_t port;
    char* executable_path;
    const char* os_name;
    vstr addr;
    lexidb db;
    ev* ev;
    vec* clients;
    vec* users;
    uint64_t cmds_processed;
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
