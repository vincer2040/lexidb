#ifndef __PARSER_H__

#define __PARSER_H__

#include "object.h"

typedef struct parser parser;

parser parser_new(const uint8_t* input, size_t input_len, int version);
object parse_object(parser* p);
int parser_has_error(parser* p);
vstr parser_get_error(parser* p);

struct parser {
    int version;
    const uint8_t* input;
    size_t input_len;
    size_t pos;
    int has_error;
    vstr error;
    uint8_t byte;
};

#endif /* __PARSER_H__ */
