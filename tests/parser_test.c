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

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("parser");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_array_cmd);
    tcase_add_test(tc_core, test_simple_string_cmd);
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
