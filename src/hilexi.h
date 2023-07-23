#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

#include <stdint.h>
#include <stddef.h>

typedef struct {
    int sfd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
    uint8_t* write_buf;
    size_t write_pos;
    size_t write_len;
} HiLexi;

HiLexi* hilexi_new(char* addr, uint16_t port);
int hilexi_connect(HiLexi* l);
int hilexi_disconnect(HiLexi* l);
int hilexi_destory(HiLexi* l);

#endif
