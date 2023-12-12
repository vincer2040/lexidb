#include "token.h"
#include <memory.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    const char* str;
    size_t str_len;
    tokent type;
} lookup;

lookup table[] = {
    { "OK", 2, Ok },
    { "PING", 4, Ping },
};

size_t table_len = sizeof table / sizeof table[0];

token lookup_simple(const uint8_t* s) {
    token tok = { 0 };
    size_t i;
    for (i = 0; i < table_len; ++i) {
        lookup l = table[i];
        if (memcmp(l.str, s, l.str_len) == 0) {
            tok.type = l.type;
            return tok;
        }
    }
    tok.type = Illegal;
    return tok;
}
