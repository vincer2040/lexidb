#ifndef __TOKEN_H__

#define __TOKEN_H__

#include <stdint.h>

typedef enum {
    Illegal,
    Eof,
    ArrayType,
    BulkType,
    Ping,
    Ok,
    Integer,
    Len,
    Bulk,
    RetCar,
    Newl,
} tokent;

typedef struct {
    tokent type;
    const uint8_t* literal;
} token;

token lookup_simple(const uint8_t* s);

#endif /* __TOKEN_H__ */
