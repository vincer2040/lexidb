#ifndef __LOGGER_H__

#define __LOGGER_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

void slowlog(FILE* stream, uint8_t* buf, size_t len);

#endif
