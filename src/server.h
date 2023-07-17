#ifndef __SERVER_H__

#define __SERVER_H__

#include "ht.h"

typedef struct {
    int sfd;
    uint32_t flags; /* mainly for padding, no use as of now */
    uint32_t port;
    uint16_t addr;
    Ht* ht;
} Server;

typedef struct {
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    Server* server;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_size;
} Connection;

int server(char* addr_str, uint16_t port);

#endif
