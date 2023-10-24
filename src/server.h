#ifndef __SERVER_H__

#define __SERVER_H__

#include "builder.h"
#include "cluster.h"
#include "config.h"
#include "ht.h"
#include "objects.h"
#include "queue.h"
#include "vec.h"

typedef struct {
    Ht* ht;
    Cluster* cluster;
    Vec* stack;
    Queue* queue;
} LexiDB;

typedef struct {
    int sfd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags; /* mainly for padding, no use as of now */
    uint16_t ismaster;
    uint16_t isslave;
    LogLevel loglevel;
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
    Connection* conn;
    LexiDB* db;
    Builder builder;
} Client;

int server(char* addr_str, uint16_t port, LogLevel loglevel, int isreplica);

void replicate(LexiDB* db, Client* slave);

#endif
