#ifndef __LOGGER_H__

#define __LOGGER_H__

#include "cmd.h"
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

void slowlog(FILE* stream, uint8_t* buf, size_t len);
void log_cmd(FILE* stream, Cmd* cmd);

#endif
