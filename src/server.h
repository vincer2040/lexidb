#ifndef __SERVER_H__

#define __SERVER_H__

#include "builder.h"
#include "cluster.h"
#include "config.h"
#include "ht.h"
#include "objects.h"
#include "queue.h"
#include "vec.h"
#include <sys/resource.h>

typedef struct {
    Ht* ht;
    Cluster* cluster;
    Vec* stack;
    Queue* queue;
} LexiDB;

typedef struct {
    uint64_t seconds;
    uint64_t microseconds;
} CmdTime;

typedef struct {
    uint64_t num_requests;
    Vec* cycles;
    Vec* times;
} ServerStats;

typedef struct {
    int sfd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags; /* mainly for padding, no use as of now */
    uint32_t ismaster;
    uint32_t isslave;
    LogLevel loglevel;
    ServerStats stats;
    LexiDB* db;
    Vec* clients;
} Server;

typedef struct {
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_size;
} Connection;

typedef struct {
    int fd;
    uint32_t isauthed;
    uint32_t ismaster;
    uint32_t isslave;
    uint64_t cycle_start;
    Connection* conn;
    LexiDB* db;
    Builder builder;
    struct timeval time_start;
} Client;

int server(char* addr_str, uint16_t port, LogLevel loglevel, int isreplica);
void replicate(LexiDB* db, Client* slave);

uint64_t rdtsc(void);
struct rusage get_cur_usage(void);
struct timeval add_user_and_sys_time(struct timeval usr, struct timeval sys);
CmdTime cmd_time(struct timeval start, struct timeval end);
int update_stats(ServerStats* stats, CmdTime* last_time, uint64_t cycles);
ServerStats stats_new(void);

#endif
