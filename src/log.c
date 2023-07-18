#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void slowlog(FILE* stream, uint8_t* buf, size_t len) {
    size_t i;
    for (i = 0; i < len; ++i) {
        fputc(buf[i], stream);
    }
}
