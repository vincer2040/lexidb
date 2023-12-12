#include "../src/lexer.h"
#include "../src/token.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t* input;
    size_t input_len;
    tokent exp[4];
} simple_test;

typedef struct {
    const uint8_t* input;
    const uint8_t* exp_len;
    const uint8_t* exp_lit;
    size_t input_len;
    size_t bulk_len;
    size_t len_len;
    tokent exp[8];
} bulk_test;

#define arr_size(arr) sizeof arr / sizeof arr[0]

START_TEST(test_simple) {
    simple_test tests[] = {
        {(const uint8_t*)"+OK\r\n", 5, {Ok, RetCar, Newl, Eof}},
        {(const uint8_t*)"+PING\r\n", 7, {Ping, RetCar, Newl, Eof}}};
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        simple_test t = tests[i];
        lexer l = lexer_new(t.input, t.input_len);
        size_t j, jlen = arr_size(t.exp);
        for (j = 0; j < jlen; ++j) {
            token tok = lexer_next_token(&l, 0);
            ck_assert_int_eq(t.exp[j], tok.type);
        }
    }
}
END_TEST

START_TEST(test_bulk) {
    bulk_test tests[] = {
        {
            .input = (const uint8_t*)"$5\r\nvince\r\n",
            .exp_len = (const uint8_t*)"5",
            .exp_lit = (const uint8_t*)"vince",
            .input_len = 11,
            .bulk_len = 5,
            .len_len = 1,
            .exp = {BulkType, Len, RetCar, Newl, Bulk, RetCar, Newl, Eof},
        },
        {
            .input = (const uint8_t*)"$7\r\nis cool\r\n",
            .exp_len = (const uint8_t*)"7",
            .exp_lit = (const uint8_t*)"is cool",
            .input_len = 13,
            .bulk_len = 7,
            .len_len = 1,
            .exp = {BulkType, Len, RetCar, Newl, Bulk, RetCar, Newl, Eof},
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        bulk_test t = tests[i];
        lexer l = lexer_new(t.input, t.input_len);
        size_t j, jlen = arr_size(t.exp);
        for (j = 0; j < jlen; ++j) {
            tokent exp = t.exp[j];
            token tok;
            if (exp == Bulk) {
                tok = lexer_next_token(&l, t.bulk_len);
                ck_assert_int_eq(exp, tok.type);
                ck_assert_mem_eq(t.exp_lit, tok.literal, t.bulk_len);
            } else if (exp == Len) {
                tok = lexer_next_token(&l, 0);
                ck_assert_int_eq(exp, tok.type);
                ck_assert_mem_eq(t.exp_len, tok.literal, t.len_len);
            } else {
                tok = lexer_next_token(&l, 0);
                ck_assert_int_eq(exp, tok.type);
            }
        }
    }
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("lexer test");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_simple);
    tcase_add_test(tc_core, test_bulk);
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
