#include "../src/vjson.c"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define arr_size(arr) sizeof arr / sizeof arr[0];

typedef struct {
    json_token_t exp_type;
    const char* exp_literal;
} token_test;

START_TEST(test_lexer) {
    const unsigned char* input = (const unsigned char*)"\
{\n\
    \"foo\": \"bar\",\n\
    \"bar\": 123,\n\
    \"baz\": -123,\n\
    \"foobar\": 123.123,\n\
    \"barbaz\": 123e12,\n\
    \"foo1\": 123E12,\n\
    \"arr\": [123, 1, \"foo\"],\n\
    \"b1\": true,\n\
    \"b2\": false,\n\
    \"n\": null\n\
}\
";
    size_t input_len = strlen((char*)input);
    json_lexer l = json_lexer_new(input, input_len);
    token_test tests[] = {
        {JT_LSquirly, NULL},    {JT_String, "foo"},    {JT_Colon, NULL},
        {JT_String, "bar"},     {JT_Comma, NULL},      {JT_String, "bar"},
        {JT_Colon, NULL},       {JT_Number, "123"},    {JT_Comma, NULL},
        {JT_String, "baz"},     {JT_Colon, NULL},      {JT_Number, "-123"},
        {JT_Comma, NULL},       {JT_String, "foobar"}, {JT_Colon, NULL},
        {JT_Number, "123.123"}, {JT_Comma, NULL},      {JT_String, "barbaz"},
        {JT_Colon, NULL},       {JT_Number, "123e12"}, {JT_Comma, NULL},
        {JT_String, "foo1"},    {JT_Colon, NULL},      {JT_Number, "123E12"},
        {JT_Comma, NULL},       {JT_String, "arr"},    {JT_Colon, NULL},
        {JT_LBracket, NULL},    {JT_Number, "123"},    {JT_Comma, NULL},
        {JT_Number, "1"},       {JT_Comma, NULL},      {JT_String, "foo"},
        {JT_RBracket, NULL},    {JT_Comma, NULL},      {JT_String, "b1"},
        {JT_Colon, NULL},       {JT_True, NULL},       {JT_Comma, NULL},
        {JT_String, "b2"},      {JT_Colon, NULL},      {JT_False, NULL},
        {JT_Comma, NULL},       {JT_String, "n"},      {JT_Colon, NULL},
        {JT_Null, NULL},        {JT_RSquirly, NULL},   {JT_Eof, NULL},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        json_token got = json_lexer_next_token(&l);
        token_test t = tests[i];
        ck_assert_int_eq(t.exp_type, got.type);
        if (t.exp_literal) {
            size_t exp_literal_len = strlen(t.exp_literal);
            size_t got_literal_len;
            ck_assert_ptr_nonnull(got.start);
            ck_assert_ptr_nonnull(got.end);
            got_literal_len = got.end - got.start;
            ck_assert_uint_eq(exp_literal_len, got_literal_len);
            ck_assert_mem_eq(t.exp_literal, got.start, exp_literal_len);
        } else {
            ck_assert_ptr_null(got.start);
            ck_assert_ptr_null(got.end);
        }
    }
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vjson");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_lexer);
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
