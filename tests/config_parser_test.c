#include "../src/config_parser.h"
#include "../src/ht.h"
#include "../src/object.h"
#include "../src/server.h"
#include <check.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define arr_size(arr) sizeof arr / sizeof arr[0]

void check_error(result(config) * config_res) {
    if (config_res->type == Err) {
        printf("%s\n", vstr_data(&config_res->data.err));
    }
    ck_assert(config_res->type != Err);
}

START_TEST(test_users) {
    const char* input = "\
# users\n\
user root >root\n\
user test >tests\n\
\n\
";
    result(config) config_res = parse_config(input, strlen(input));
    config config;
    user *root, *test;
    check_error(&config_res);
    config = config_res.data.ok;
    ck_assert_uint_eq(config.users->len, 2);
    root = vec_get_at(config.users, 0);
    ck_assert_ptr_nonnull(root);
    ck_assert_str_eq(vstr_data(&root->name), "root");
    ck_assert_str_eq(vstr_data(&root->password), "root");
    test = vec_get_at(config.users, 1);
    ck_assert_ptr_nonnull(test);
    ck_assert_str_eq(vstr_data(&test->name), "test");
    ck_assert_str_eq(vstr_data(&test->password), "tests");
    config_free(&config);
}
END_TEST

START_TEST(test_databases) {
    const char* input = "\
# databases\n\
# the number of databases for lexidb to use\n\
databases 16\n\
\n\
";
    result(config) config_res = parse_config(input, strlen(input));
    config config;
    check_error(&config_res);
    config = config_res.data.ok;
    ck_assert_uint_eq(config.databases, 16);
    config_free(&config);
}
END_TEST

START_TEST(test_address) {
    const char* input = "\
# address\n\
address 127.0.0.1\n\
\n\
";
    result(config) config_res = parse_config(input, strlen(input));
    config config;
    check_error(&config_res);
    config = config_res.data.ok;
    ck_assert_str_eq(vstr_data(&config.address), "127.0.0.1");
    config_free(&config);
}
END_TEST

START_TEST(test_port) {
    const char* input = "\
# port\n\
port 1234\n\
\n\
";
    result(config) config_res = parse_config(input, strlen(input));
    config config;
    check_error(&config_res);
    config = config_res.data.ok;
    ck_assert_uint_eq(config.port, 1234);
    config_free(&config);
}
END_TEST

START_TEST(test_all) {
    const char* input = "\
# users\n\
user root >root\n\
user test >tests\n\
\n\
# databases\n\
# the number of databases for lexidb to use\n\
databases 16\n\
\n\
# address\n\
address 127.0.0.1\n\
\n\
# port\n\
port 1234\n\
\n\
";
    result(config) config_res = parse_config(input, strlen(input));
    config config;
    user *root, *test;
    check_error(&config_res);
    config = config_res.data.ok;

    ck_assert_uint_eq(config.users->len, 2);
    root = vec_get_at(config.users, 0);
    ck_assert_ptr_nonnull(root);
    ck_assert_str_eq(vstr_data(&root->name), "root");
    ck_assert_str_eq(vstr_data(&root->password), "root");
    test = vec_get_at(config.users, 1);
    ck_assert_ptr_nonnull(test);
    ck_assert_str_eq(vstr_data(&test->name), "test");
    ck_assert_str_eq(vstr_data(&test->password), "tests");

    ck_assert_uint_eq(config.databases, 16);

    ck_assert_str_eq(vstr_data(&config.address), "127.0.0.1");

    ck_assert_uint_eq(config.port, 1234);

    config_free(&config);
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("config parser");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_users);
    tcase_add_test(tc_core, test_databases);
    tcase_add_test(tc_core, test_address);
    tcase_add_test(tc_core, test_port);
    tcase_add_test(tc_core, test_all);
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
