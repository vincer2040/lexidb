#ifndef __LOG_H__

#define __LOG_H__

#include "util.h"
#include <stdio.h>
#include <stdlib.h>

#define info(...)                                                              \
    do {                                                                       \
        struct timespec time = get_time();                                     \
        struct tm* tm = localtime(&time.tv_sec);                               \
        printf("%d-%d-%d %02d:%02d:%02d", tm->tm_mon + 1, tm->tm_mday,         \
               tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);       \
        printf("\033[32m INFO  \033[39m");                                     \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    } while (0)

#define warn(...)                                                              \
    do {                                                                       \
        struct timespec time = get_time();                                     \
        struct tm* tm = localtime(&time.tv_sec);                               \
        printf("%d-%d-%d %02d:%02d:%02d", tm->tm_mon + 1, tm->tm_mday,         \
               tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);       \
        printf("\033[33m WARN  \033[39m");                                     \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    } while (0)

#define error(...)                                                             \
    do {                                                                       \
        struct timespec time = get_time();                                     \
        struct tm* tm = localtime(&time.tv_sec);                               \
        printf("%d-%d-%d %02d:%02d:%02d", tm->tm_mon + 1, tm->tm_mday,         \
               tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);       \
        fprintf(stderr, "\033[31m ERROR \033[39m");                            \
        fprintf(stderr, __VA_ARGS__);                                          \
        fflush(stderr);                                                        \
    } while (0)

#define debug(...)                                                             \
    do {                                                                       \
        struct timespec time = get_time();                                     \
        struct tm* tm = localtime(&time.tv_sec);                               \
        printf("%d-%d-%d %02d:%02d:%02d", tm->tm_mon + 1, tm->tm_mday,         \
               tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);       \
        printf("\033[34m DEBUG \033[39m");                                     \
        printf("%s:%d ", __FILE__, __LINE__);                                  \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    } while (0)

#endif /* __LOG_H__ */
