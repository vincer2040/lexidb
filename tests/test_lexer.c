#include "../src/lexer.h"
#include "../src/token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define assert_uint_eq(a, b)                                                   \
    {                                                                          \
        if ((a) != (b)) {                                                      \
            fprintf(stderr, "%s:%d, %u != %u\n", __FILE__, __LINE__, (a),      \
                    (b));                                                      \
            abort();                                                           \
        }                                                                      \
    }

#define assert_mem_eq(a, b, size)                                              \
    {                                                                          \
        if (memcmp((a), (b), (size)) != 0) {                                   \
            fprintf(stderr, "%s:%d, %s != %s\n", __FILE__, __LINE__, (a),      \
                    (b));                                                      \
            abort();                                                           \
        }                                                                      \
    }

void t1() {
    Token toks[] = {
        {.type = TYPE, .literal = ((uint8_t*)"*")},
        {.type = LEN, .literal = ((uint8_t*)"2")},
        {.type = RETCAR, .literal = ((uint8_t*)"\r")},
        {.type = NEWL, .literal = ((uint8_t*)"\n")},
        {.type = TYPE, .literal = ((uint8_t*)"$")},
        {.type = LEN, .literal = ((uint8_t*)"3")},
        {.type = RETCAR, .literal = ((uint8_t*)"\r")},
        {.type = NEWL, .literal = ((uint8_t*)"\n")},
        {.type = BULK, .literal = ((uint8_t*)"SET")},
        {.type = RETCAR, .literal = ((uint8_t*)"\r")},
        {.type = NEWL, .literal = ((uint8_t*)"\n")},
        {.type = TYPE, .literal = ((uint8_t*)"$")},
        {.type = LEN, .literal = ((uint8_t*)"5")},
        {.type = RETCAR, .literal = ((uint8_t*)"\r")},
        {.type = NEWL, .literal = ((uint8_t*)"\n")},
        {.type = BULK, .literal = ((uint8_t*)"vince")},
        {.type = RETCAR, .literal = ((uint8_t*)"\r")},
        {.type = NEWL, .literal = ((uint8_t*)"\n")},
        {.type = EOFT, .literal = ((uint8_t*)"\0")},
    };

    uint8_t* input = ((uint8_t*)"*2\r\n$3\r\nSET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));
    size_t len = sizeof(toks) / sizeof(toks[0]);
    size_t i;

    Lexer l;
    Token tok;

    l = lexer_new(input, inp_len);

    for (i = 0; i < len; ++i) {
        tok = lexer_next_token(&l);
        assert_uint_eq(tok.type, toks[i].type);
    }

    printf("lexer test 1 passed (it works)\n");
}

void t2() {
    Token toks[] = {
        { .type = PING, .literal = ((uint8_t*)"+PING\r\n") },
    };
    uint8_t* input = ((uint8_t*)"+PING\r\n");
    size_t inp_len = strlen(((char*)input));
    size_t len = sizeof(toks) / sizeof(toks[0]);
    size_t i;

    Lexer l;
    Token tok;

    l = lexer_new(input, inp_len);

    for (i = 0; i < len; ++i) {
        tok = lexer_next_token(&l);
        assert_uint_eq(tok.type, toks[i].type);
    }

    printf("lexer test 2 passed (simple strings)\n");
}

int main(void) {
    t1();
    t2();
    printf("all lexer tests passed\n");
    return 0;
}
