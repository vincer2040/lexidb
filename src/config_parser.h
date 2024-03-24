#ifndef __CONFIG_PARSER_H__

#define __CONFIG_PARSER_H__

#include "ht.h"
#include "result.h"
#include "vec.h"

typedef enum {
    Address,
    Port,
    User,
    Databases,
} line_data_type;

typedef struct {
    uint16_t port;
    size_t databases;
    vstr address;
    vec* users;
} config;

result_t(config, vstr);

result(config) parse_config(const char* input, size_t input_len);
void config_free(config* config);

#endif /* __CONFIG_PARSER_H__ */
