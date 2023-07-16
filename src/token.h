#ifndef __TOKEN_H__

#define __TOKEN_H__

#include <stdint.h>

typedef enum {
    EOFT,
    PING,
    TYPE,
    LEN,
    RETCAR,
    NEWL,
    BULK,
    ILLEGAL
} TokenT;

typedef struct {
    TokenT type;
    uint8_t* literal;
} Token;

#endif
