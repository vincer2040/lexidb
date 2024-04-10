#include "../src/object.h"
#include "../src/parser.h"
#include "../src/vstr.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>

#define arr_size(tests) sizeof tests / sizeof tests[0]

typedef struct {
    const char* input;
    const char* exp;
} string_test;

typedef struct {
    const char* input;
    int64_t exp;
} int_test;

typedef struct {
    const char* input;
    double exp;
} double_test;

typedef struct {
    const char* input;
    int exp;
} boolean_test;

void check_error(parser* p) {
    if (parser_has_error(p)) {
        printf("%s\n", vstr_data(&p->error));
        ck_assert(0 && "parser had error");
    }
}

void assert_object_string(object* got, const char* exp) {
    ck_assert_int_eq(got->type, OT_String);
    ck_assert_str_eq(vstr_data(&got->data.string), exp);
}

void assert_object_error(object* got, const char* exp) {
    ck_assert_int_eq(got->type, OT_Error);
    ck_assert_str_eq(vstr_data(&got->data.string), exp);
}

void assert_object_int(object* got, int64_t exp) {
    ck_assert_int_eq(got->type, OT_Int);
    ck_assert_int_eq(got->data.integer, exp);
}

void assert_object_double(object* got, double exp) {
    ck_assert_int_eq(got->type, OT_Double);
    ck_assert_double_eq(got->data.dbl, exp);
}

void assert_object_boolean(object* got, int exp) {
    ck_assert_int_eq(got->type, OT_Boolean);
    ck_assert_int_eq(got->data.integer, exp);
}

void assert_object_null(object* got) { ck_assert_int_eq(got->type, OT_Null); }

START_TEST(test_simple_string) {
    string_test tests[] = {
        {
            "+OK\r\n",
            "OK",
        },
        {
            "+NONE\r\n",
            "NONE",
        },
        {
            "+INFO\r\n",
            "INFO",
        },
        {
            "+SET\r\n",
            "SET",
        },
        {
            "+GET\r\n",
            "GET",
        },
        {
            "+123456789\r\n",
            "123456789",
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_string(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_integers) {
    int_test tests[] = {
        {
            ":123\r\n",
            123,
        },
        {
            ":-123\r\n",
            -123,
        },
        {
            ":+123\r\n",
            123,
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        int_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_int(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_doubles) {
    double_test tests[] = {
        {
            ",123.123\r\n",
            123.123,
        },
        {
            ",-123.123\r\n",
            -123.123,
        },
        {
            ",+123.123\r\n",
            123.123,
        },
        {
            ",+123e2\r\n",
            12300,
        },
        {
            ",123e-2\r\n",
            1.23,
        },
        {
            ",123e-5\r\n",
            0.00123,
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        double_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_double(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_booleans) {
    boolean_test tests[] = {
        {
            "#t\r\n",
            1,
        },
        {
            "#f\r\n",
            0,
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        boolean_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_boolean(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_null) {
    const char* test = "_\r\n";
    parser p = parser_new((const uint8_t*)test, strlen(test), 1);
    object parsed = parse_object(&p);
    check_error(&p);
    assert_object_null(&parsed);
    object_free(&parsed);
}
END_TEST

START_TEST(test_simple_error) {
    string_test tests[] = {
        {
            "-Err\r\n",
            "Err",
        },
        {
            "-unkown command: 'asdf'\r\n",
            "unkown command: 'asdf'",
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_error(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_bulk_strings) {
    string_test tests[] = {
        {
            "$3\r\nfoo\r\n",
            "foo",
        },
        {
            "$5\r\nfoo\r\n\r\n",
            "foo\r\n",
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_string(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

START_TEST(test_bulk_errors) {
    string_test tests[] = {
        {
            "!3\r\nfoo\r\n",
            "foo",
        },
        {
            "!5\r\nfoo\r\n\r\n",
            "foo\r\n",
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        string_test test = tests[i];
        parser p =
            parser_new((const uint8_t*)test.input, strlen(test.input), 1);
        object parsed = parse_object(&p);
        check_error(&p);
        assert_object_error(&parsed, test.exp);
        object_free(&parsed);
    }
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("parser");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_simple_string);
    tcase_add_test(tc_core, test_integers);
    tcase_add_test(tc_core, test_doubles);
    tcase_add_test(tc_core, test_booleans);
    tcase_add_test(tc_core, test_null);
    tcase_add_test(tc_core, test_simple_error);
    tcase_add_test(tc_core, test_bulk_strings);
    tcase_add_test(tc_core, test_bulk_errors);
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
