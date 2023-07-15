#ifndef __TOKEN_H__

#define __TOKEN_H__

#include <stdint.h>

typedef enum { EOFT, TYPE, LEN, RETCAR, NEWL, BULK } TokenT;

typedef struct {
    TokenT type;
    uint8_t* literal;
} Token;

#endif
