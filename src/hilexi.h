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
int hilexi_authenticate(hilexi *l, const char* username, const char* password);
int hilexi_connect(hilexi* l);

result(object) hilexi_ping(hilexi* l);
result(object) hilexi_info(hilexi* l);
result(object) hilexi_keys(hilexi* l);
result(object) hilexi_set(hilexi* l, object* key, object* value);
result(object) hilexi_get(hilexi* l, object* key);
result(object) hilexi_del(hilexi* l, object* key);
result(object) hilexi_push(hilexi* l, object* value);
result(object) hilexi_pop(hilexi* l);
result(object) hilexi_enque(hilexi* l, object* value);
result(object) hilexi_deque(hilexi* l);
result(object) hilexi_zset(hilexi* l, object* value);
result(object) hilexi_zhas(hilexi* l, object* value);
result(object) hilexi_zdel(hilexi* l, object* value);

void hilexi_close(hilexi* l);

#endif /* __HI_LEXI_H__ */
