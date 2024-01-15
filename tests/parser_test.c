#include "../src/builder.h"
#include "../src/cmd.h"
#include "../src/object.h"
#include "../src/parser.h"
#include <check.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t* input;
    size_t input_len;
    cmd exp;
} array_cmd_test;

typedef struct {
    const uint8_t* input;
    size_t input_len;
    cmd exp;
} simple_cmd_test;

typedef struct {
    const uint8_t* input;
    size_t input_len;
    const char* exp_string;
} string_from_server_test;

typedef struct {
    const uint8_t* input;
    size_t input_len;
    double exp;
} double_test;

typedef struct {
    const uint8_t* input;
    size_t input_len;
    cmd exp;
} help_cmd_test;

#define test_object_eq(exp, got)                                               \
    do {                                                                       \
        int cmp = object_cmp(&exp, &got);                                      \
        ck_assert_int_eq(cmp, 0);                                              \
    } while (0)

#define test_set_cmd(exp, cmd)                                                 \
    do {                                                                       \
        ck_assert(cmd.type == Set);                                            \
        test_object_eq(exp.data.set.key, cmd.data.set.key);                    \
        test_object_eq(exp.data.set.value, cmd.data.set.value);                \
    } while (0)

#define test_get_cmd(exp, cmd)                                                 \
    do {                                                                       \
        ck_assert(cmd.type == Get);                                            \
        test_object_eq(exp.data.get.key, cmd.data.get.key);                    \
    } while (0)

#define test_del_cmd(exp, cmd)                                                 \
    do {                                                                       \
        ck_assert(cmd.type == Del);                                            \
        test_object_eq(exp.data.get.key, cmd.data.get.key);                    \
    } while (0)

#define test_ping_cmd(cmd)                                                     \
    do {                                                                       \
        ck_assert(cmd.type == Ping);                                           \
    } while (0)

#define test_cmd(exp, cmd)                                                     \
    do {                                                                       \
        switch (exp.type) {                                                    \
        case Okc:                                                              \
            ck_assert(cmd.type == Okc);                                        \
            break;                                                             \
        case Set:                                                              \
            test_set_cmd(exp, cmd);                                            \
            break;                                                             \
        case Get:                                                              \
            test_get_cmd(exp, cmd);                                            \
            break;                                                             \
        case Del:                                                              \
            test_del_cmd(exp, cmd);                                            \
            break;                                                             \
        case Ping:                                                             \
            test_ping_cmd(cmd);                                                \
            break;                                                             \
        default:                                                               \
            ck_assert(0 && "invalid cmd");                                     \
        }                                                                      \
    } while (0)

#define arr_size(arr) sizeof arr / sizeof arr[0]

START_TEST(test_array_cmd) {
    vstr k1 = vstr_from("foo");
    vstr v1 = vstr_from("bar");
    vstr k2 = vstr_from("foo123");
    vstr k3 = vstr_from("foo1234567");
    vstr k4 = vstr_from("foo\r\n");
    array_cmd_test tests[] = {
        {
            (const uint8_t*)"*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n",
            strlen("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"),
            {
                Set,
                {
                    object_new(String, &k1),
                    object_new(String, &v1),
                },
            },
        },
        {
            (const uint8_t*)"*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n",
            strlen("*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n"),
            {
                Get,
                {
                    object_new(String, &k1),
                },
            },
        },
        {
            (const uint8_t*)"*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n",
            strlen("*2\r\n$3\r\nDEL\r\n$3\r\nfoo\r\n"),
            {
                Del,
                {
                    object_new(String, &k1),
                },
            },
        },
        {
            (const uint8_t*)"*2\r\n$3\r\nGET\r\n$6\r\nfoo123\r\n",
            strlen("*2\r\n$3\r\nGET\r\n$6\r\nfoo123\r\n"),
            {
                Get,
                {
                    object_new(String, &k2),
                },
            },
        },
        {
            (const uint8_t*)"*2\r\n$3\r\nGET\r\n$10\r\nfoo1234567\r\n",
            strlen("*2\r\n$3\r\nGET\r\n$10\r\nfoo1234567\r\n"),
            {
                Get,
                {
                    object_new(String, &k3),
                },
            },
        },
        {
            (const uint8_t*)"*2\r\n$3\r\nGET\r\n$5\r\nfoo\r\n\r\n",
            strlen("*2\r\n$3\r\nGET\r\n$5\r\nfoo\r\n\r\n"),
            {
                Get,
                {
                    object_new(String, &k4),
                },
            },
        },
    };

    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        array_cmd_test t = tests[i];
        cmd c = parse(t.input, t.input_len);
        test_cmd(t.exp, c);
    }
}
END_TEST

START_TEST(test_simple_string_cmd) {
    simple_cmd_test tests[] = {
        {
            (const uint8_t*)"+PING\r\n",
            strlen("+PING\r\n"),
            {Ping},
        },
        {
            (const uint8_t*)"+OK\r\n",
            strlen("+OK\r\n"),
            {Okc},
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        simple_cmd_test t = tests[i];
        cmd cmd = parse(t.input, t.input_len);
        test_cmd(t.exp, cmd);
    }
}
END_TEST

START_TEST(test_parse_string_from_server) {
    string_from_server_test tests[] = {
        {
            (const uint8_t*)"$3\r\nfoo\r\n",
            strlen("$3\r\nfoo\r\n"),
            "foo",
        },
        {
            (const uint8_t*)"+PONG\r\n",
            strlen("+PONG\r\n"),
            "PONG",
        },
        {
            (const uint8_t*)"+OK\r\n",
            strlen("+OK\r\n"),
            "OK",
        },
        {
            (const uint8_t*)"$5\r\nfoo\r\n\r\n",
            strlen("$3\r\nfoo\r\n\r\n"),
            "foo\r\n",
        },
    };
    size_t i, len = arr_size(tests);

    for (i = 0; i < len; ++i) {
        string_from_server_test t = tests[i];
        object obj = parse_from_server(t.input, t.input_len);
        ck_assert(obj.type == String);
        vstr d = obj.data.string;
        ck_assert_str_eq(t.exp_string, vstr_data(&d));
    }
}
END_TEST

START_TEST(test_parse_int_from_server) {
    int64_t tests[] = {
        1337,
        -1337,
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        int64_t t = tests[i];
        builder b = builder_new();
        const uint8_t* out;
        size_t out_len;
        object parsed;
        int64_t got;
        builder_add_int(&b, t);

        out = builder_out(&b);
        out_len = builder_len(&b);
        parsed = parse_from_server(out, out_len);
        ck_assert(parsed.type == Int);

        got = parsed.data.num;
        ck_assert_int_eq(got, t);
    }
}
END_TEST

START_TEST(test_parse_array) {
    uint8_t* input = (uint8_t*)"*2\r\n*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n*2\r\n$"
                               "3\r\nbaz\r\n$6\r\nfoobar\r\n";
    size_t len = strlen((char*)input);
    object parsed = parse_from_server(input, len);
    vec *arr, *a0_vec, *a1_vec;
    object *a0, *a1, *a01, *a02, *a11, *a12;
    ck_assert(parsed.type == Array);
    arr = parsed.data.vec;
    ck_assert_uint_eq(arr->len, 2);

    a0 = vec_get_at(arr, 0);
    ck_assert(a0->type == Array);
    a0_vec = a0->data.vec;
    ck_assert_uint_eq(a0_vec->len, 2);
    a01 = vec_get_at(a0_vec, 0);
    ck_assert(a01->type == String);
    ck_assert_str_eq(vstr_data(&a01->data.string), "foo");
    a02 = vec_get_at(a0_vec, 1);
    ck_assert(a02->type == String);
    ck_assert_str_eq(vstr_data(&a02->data.string), "bar");

    a1 = vec_get_at(arr, 1);
    ck_assert(a1->type == Array);
    a1_vec = a1->data.vec;
    ck_assert_uint_eq(a1_vec->len, 2);
    a11 = vec_get_at(a1_vec, 0);
    ck_assert(a11->type == String);
    ck_assert_str_eq(vstr_data(&a11->data.string), "baz");
    a12 = vec_get_at(a1_vec, 1);
    ck_assert(a12->type == String);
    ck_assert_str_eq(vstr_data(&a12->data.string), "foobar");

    object_free(&parsed);
}
END_TEST

START_TEST(test_parse_double) {
    double_test tests[] = {
        {
            (const uint8_t*)",123.123\r\n",
            strlen(",123.123\r\n"),
            123.123,
        },
        {
            (const uint8_t*)",123e10\r\n",
            strlen(",123e10\r\n"),
            1230000000000,
        },
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        double_test t = tests[i];
        object parsed = parse_from_server(t.input, t.input_len);
        ck_assert_uint_eq(parsed.type, Double);
        ck_assert_double_eq(parsed.data.dbl, t.exp);
    }
}
END_TEST

START_TEST(test_parse_help_cmd) {
    help_cmd_test tests[] = {
        {(const uint8_t*)"*2\r\n$4\r\nHELP\r\n$3\r\nSET\r\n",
         strlen("*2\r\n$4\r\nHELP\r\n$3\r\nSET\r\n"),
         {
             Help,
             {
                 1,
                 Set,
             },
         }},
        {(const uint8_t*)"$4\r\nHELP\r\n",
         strlen("$4\r\nHELP\r\n"),
         {
             Help,
             {0},
         }},
    };
    size_t i, len = arr_size(tests);
    for (i = 0; i < len; ++i) {
        help_cmd_test t = tests[i];
        cmd parsed = parse(t.input, t.input_len);
        cmd exp = t.exp;
        ck_assert_int_eq(parsed.type, exp.type);
        ck_assert_int_eq(parsed.data.help.wants_cmd_help,
                         exp.data.help.wants_cmd_help);
        if (exp.data.help.wants_cmd_help) {
            ck_assert_int_eq(parsed.data.help.wants_cmd_help,
                             exp.data.help.wants_cmd_help);
        }
    }
}

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("parser");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_array_cmd);
    tcase_add_test(tc_core, test_simple_string_cmd);
    tcase_add_test(tc_core, test_parse_string_from_server);
    tcase_add_test(tc_core, test_parse_int_from_server);
    tcase_add_test(tc_core, test_parse_array);
    tcase_add_test(tc_core, test_parse_double);
    tcase_add_test(tc_core, test_parse_help_cmd);
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
