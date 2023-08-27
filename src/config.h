#ifndef __CONFIG_H__

#define __CONFIG_H__

typedef enum {
    LL_INV = 0,
    LL_NONE = 1,
    LL_INFO = 2,
    LL_CMD = 3,
    LL_DEBUG = 4,
    LL_VERBOSE = 5,
} LogLevel;

typedef struct {
    char* logfile; // path to logfile
    char* conf; // path to config file
    LogLevel loglevel;
} Config;

void configure_from_args(int argc, char** argv);

#endif /* __CONFIG_H__ */
