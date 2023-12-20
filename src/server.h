#ifndef __SERVER_H__

#define __SERVER_H__

#define _XOPEN_SOURCE 600
#include "builder.h"
#include "ev.h"
#include "ht.h"
#include "vec.h"
#include "vstr.h"
#include <stdint.h>
#include <time.h>

int server_run(const char* addr, uint16_t port);

#endif /* __SERVER_H__ */
