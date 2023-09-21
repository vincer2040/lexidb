#include "../src/lexer.h"
#include "../src/token.h"
#include "../src/builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

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

START_TEST(test_it_works) {
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
}
END_TEST

START_TEST(test_simple_string) {
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
}
END_TEST

START_TEST(test_integers) {
    Builder b = builder_create(11);
    Token toks[] = {
        { .type = INT, .literal = ((uint8_t*)":") },
        { .type = RETCAR, .literal = ((uint8_t*)"\r") },
        { .type = NEWL, .literal = ((uint8_t*)"\n") },
    };
    size_t input_len, i;
    size_t len = (sizeof toks) / (sizeof toks[0]);
    uint8_t* buf;

    Lexer l;
    Token tok;

    builder_add_int(&b, 42069);

    input_len = b.ins;
    buf = builder_out(&b);

    l = lexer_new(buf, input_len);

    for (i = 0; i < len; ++i) {
        tok = lexer_next_token(&l);
        assert_uint_eq(tok.type, toks[i].type);
    }

    free(buf);

    printf("lexer test 3 passed (integers)\n");
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("lexer_test");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_simple_string);
    tcase_add_test(tc_core, test_integers);
    suite_add_tcase(s, tc_core);
    return s;
}

int main() {
    int number_failed;
    Suite* s;
    SRunner* sr;
    s = suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
