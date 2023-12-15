#include "vstr.h"
#include <stdio.h>

vstr read_line(void) {
    vstr s = vstr_new();
    char ch;
    while ((ch = fgetc(stdin)) != '\n') {
        int p = vstr_push_char(&s, ch);
        if (p == -1) {
            vstr_reset(&s);
            return s;
        }
    }
    return s;
}
