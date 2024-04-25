#include "../src/builder.h"
#include <check.h>
#include <stdlib.h>

typedef struct {
    const char* str;
    size_t str_len;
    const char* exp;
} string_test;

typedef struct {
    int64_t value;
    const char* exp;
} int_test;

typedef struct {
    double dbl;
    const char* exp;
} dbl_test;

typedef struct {
    size_t len;
    const char* exp;
} arr_test;

#define arr_size(arr) sizeof arr / sizeof arr[0]

START_TEST(test_simple_strings) {
    string_test tests[] = {
        {"OK", 2, "+OK\r\n"},
        {"PING", 4, "+PING\r\n"},
        {"NONE", 4, "+NONE\r\n"},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test t = tests[i];
        builder b = builder_new();
        ck_assert_int_eq(builder_add_simple_string(&b, t.str, t.str_len), 0);
        const unsigned char* out = builder_out(&b);
        size_t out_len = builder_len(&b);
        size_t exp_len = strlen(t.exp);
        ck_assert_uint_eq(out_len, exp_len);
        ck_assert_mem_eq(t.exp, out, exp_len);
    }
}
END_TEST

START_TEST(test_bulk_strings) {
    string_test tests[] = {
        {"foo\nbar\n", 8, "$8\r\nfoo\nbar\n\r\n"},
        {"foo\nbar\nbaz", 11, "$11\r\nfoo\nbar\nbaz\r\n"}
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test t = tests[i];
        builder b = builder_new();
        ck_assert_int_eq(builder_add_bulk_string(&b, t.str, t.str_len), 0);
        const unsigned char* out = builder_out(&b);
        size_t out_len = builder_len(&b);
        size_t exp_len = strlen(t.exp);
        ck_assert_uint_eq(out_len, exp_len);
        ck_assert_mem_eq(t.exp, out, exp_len);
    }
}
END_TEST

START_TEST(test_ints) {
    int_test tests[] = {
        {2, ":2\r\n"},
        {24, ":24\r\n"},
        {-2, ":-2\r\n"},
        {-24, ":-24\r\n"},
        {9223372036854775807, ":9223372036854775807\r\n"},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        int_test t = tests[i];
        builder b = builder_new();
        ck_assert_int_eq(builder_add_int(&b, t.value), 0);
        const unsigned char* out = builder_out(&b);
        size_t out_len = builder_len(&b);
        size_t exp_len = strlen(t.exp);
        ck_assert_uint_eq(out_len, exp_len);
        ck_assert_mem_eq(t.exp, out, exp_len);
    }
}
END_TEST

START_TEST(test_doubles) {
    dbl_test tests[] = {
        {2, ",2\r\n"},
        {24.24, ",24.24\r\n"},
        {.24, ",0.24\r\n"},
        {-2, ",-2\r\n"},
        {-24.24, ",-24.24\r\n"},
        {-.24, ",-0.24\r\n"},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        dbl_test t = tests[i];
        builder b = builder_new();
        ck_assert_int_eq(builder_add_double(&b, t.dbl), 0);
        const unsigned char* out = builder_out(&b);
        size_t out_len = builder_len(&b);
        size_t exp_len = strlen(t.exp);
        ck_assert_uint_eq(out_len, exp_len);
        ck_assert_mem_eq(t.exp, out, exp_len);
    }
}
END_TEST

START_TEST(test_arrays) {
    arr_test tests[] = {
        {2, "*2\r\n"},
        {10, "*10\r\n"},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        arr_test t = tests[i];
        builder b = builder_new();
        ck_assert_int_eq(builder_add_array(&b, t.len), 0);
        const unsigned char* out = builder_out(&b);
        size_t out_len = builder_len(&b);
        size_t exp_len = strlen(t.exp);
        ck_assert_uint_eq(out_len, exp_len);
        ck_assert_mem_eq(t.exp, out, exp_len);
    }
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("builder");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_simple_strings);
    tcase_add_test(tc_core, test_bulk_strings);
    tcase_add_test(tc_core, test_ints);
    tcase_add_test(tc_core, test_doubles);
    tcase_add_test(tc_core, test_arrays);
    suite_add_tcase(s, tc_core);
    return s;
}

int main(void) {
    int num_failed;
    Suite* s;
    SRunner* sr;
    s = suite();
    sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (num_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
