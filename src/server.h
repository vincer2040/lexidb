#ifndef __SERVER_H__

#define __SERVER_H__

#include "ht.h"
#include "cluster.h"
#include "objects.h"
#include "vec.h"

typedef struct {
    Ht* ht;
    Cluster* cluster;
    Vec* stack;
}LexiDB;

typedef struct {
    int sfd;
    uint32_t port;
    uint16_t addr;
    uint16_t flags; /* mainly for padding, no use as of now */
    uint32_t ismaster;
    LexiDB* db;
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
    Connection* conn;
    LexiDB* db;
} Client;

int server(char* addr_str, uint16_t port);

#endif
