#ifndef __LOGGER_H__

#define __LOGGER_H__

#include "cmd.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define LOG_INFO "INFO] "
#define LOG_ERROR "ERROR] "
#define LOG_CONNECTION "CONNECTION] "
#define LOG_CLOSE "CLOSE] "

#define LOG(...)                                                               \
    {                                                                          \
        time_t t = time(NULL);                                                 \
        struct tm tm = *localtime(&t);                                         \
        printf("[%d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900,             \
               tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);   \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    }

void slowlog(uint8_t* buf, size_t len);
void log_cmd(Cmd* cmd);

#endif
