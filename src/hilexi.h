#ifndef __HI_LEXI_H__

#define __HI_LEXI_H__

#include "builder.h"
#include "object.h"
#include "result.h"
#include "vstr.h"

typedef struct {
    int sfd;
    uint32_t addr;
    uint16_t port;
    uint16_t flags;
    builder builder;
    uint8_t* read_buf;
    size_t read_pos;
    size_t read_cap;
} hilexi;

result_t(hilexi, vstr);
result_t(object, vstr);

result(hilexi) hilexi_new(const char* addr, uint16_t port);
object hilexi_ping(hilexi* l);
int hilexi_connect(hilexi* l);
void hilexi_close(hilexi* l);

#endif /* __HI_LEXI_H__ */
