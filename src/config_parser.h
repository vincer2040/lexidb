#ifndef __CONFIG_PARSER_H__

#define __CONFIG_PARSER_H__

#include "ht.h"
#include "result.h"

typedef enum {
    Address,
    Port,
    User,
} line_data_type;

result_t(ht, vstr);

result(ht) parse_config(const char* input, size_t input_len);
void config_free(ht* config);

#endif /* __CONFIG_PARSER_H__ */
