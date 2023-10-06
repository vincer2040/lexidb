#ifndef __CONFIG_H__

#define __CONFIG_H__

#include "ht.h"
#include "vec.h"

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
    char* conf;    // path to config file
    LogLevel loglevel;
} Config;

typedef enum {
    COT_STRING,
    COT_INT,
    COT_NULL,
} ConfigOptionType;

typedef struct {
    const char* arg;
    const char* short_arg;
    ConfigOptionType type;
    union {
        char* str;
        int integer;
    } default_value;
    const char* metadata;
} ConfigOption;

typedef Vec Configuration;

Configuration* config_new(void);
int config_add_option(Configuration** config, const char* arg,
                      const char* short_arg, ConfigOptionType type,
                      void* default_value, const char* metadata);
Ht* configure(Configuration* config, int argc, char** argv);
void config_free(Configuration* config);
void free_configuration_ht(Ht* ht);
LogLevel determine_loglevel(char* level);

#endif /* __CONFIG_H__ */
