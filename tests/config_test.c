#include "../src/server.h"
#include "../src/vstr.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>

#define arr_size(arr) sizeof arr / sizeof arr[0]

START_TEST(test_bind) {
    const char* input = "\
# specify the address to bind to\n\
bind 127.0.0.1\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_str_eq(vstr_data(&s.bind_addr), "127.0.0.1");
    vec_free(s.users, NULL);
}
END_TEST

START_TEST(test_protected_mode) {
    const char* input = "\
# run in protected mode\n\
# specify if you want to run in protected mode,\n\
# requiring authenication of connected clients\n\
protected-mode yes\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_int_eq(s.protected_mode, 1);
    vec_free(s.users, NULL);
}
END_TEST

START_TEST(test_port) {
    const char* input = "\
# specify ports to accept connections on (default 5173)\n\
port 5173\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_uint_eq(s.port, 5173);
    vec_free(s.users, NULL);

}
END_TEST

START_TEST(test_tcp_backlog) {
    const char* input = "\
# tcp listen backlog\n\
tcp-backlog 511\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_int_eq(s.tcp_backlog, 511);
    vec_free(s.users, NULL);

}
END_TEST

START_TEST(test_loglevel) {
    const char* input = "\
# log level\n\
# can be one of\n\
# nothing\n\
# info\n\
# warning\n\
# verbose\n\
# debug\n\
loglevel info\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_int_eq(s.loglevel, LL_Info);
    vec_free(s.users, NULL);
}
END_TEST

START_TEST(test_logfile) {
    const char* input = "\
# logfile\n\
# empty string will result in logging being sent to stdout\n\
# note that one should remove the "" when providing an actual\n\
# file path\n\
logfile \"\"\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_ptr_null(s.logfile);
    vec_free(s.users, NULL);
}
END_TEST

START_TEST(test_databases) {
    const char* input = "\
# the number of databases\n\
# default database is DB 0\n\
databases 16\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_int_eq(s.num_databases, 16);
    vec_free(s.users, NULL);

}
END_TEST

START_TEST(test_user) {
    const char* input = "\
user username on +$connection +set >password\n\
";
    size_t input_len = strlen(input);
    lexi_server s = {0};
    vstr err = {0};
    int res;
    const user* u;
    const vstr* password;
    const category* cat;
    const cmd_type* cmd;
    s.users = vec_new(sizeof(user));
    res = configure_server(&s, input, input_len, &err);
    if (res == -1) {
        printf("error: %s\n", vstr_data(&err));
        ck_assert(0 && "configure_lexi_server had error");
    }
    ck_assert_uint_eq(vec_len(s.users), 1);
    u = vec_get_at(s.users, 0);
    ck_assert_ptr_nonnull(u);
    ck_assert_str_eq(vstr_data(&u->username), "username");
    ck_assert(u->flags & USER_ON);
    ck_assert_uint_eq(vec_len(u->passwords), 1);
    password = vec_get_at(u->passwords, 0);
    ck_assert_ptr_nonnull(password);
    ck_assert_str_eq(vstr_data(password), "password");
    ck_assert_uint_eq(vec_len(u->categories), 1);
    cat = vec_get_at(u->categories, 0);
    ck_assert_ptr_nonnull(cat);
    ck_assert_int_eq(*cat, C_Connection);
    ck_assert_uint_eq(vec_len(u->commands), 1);
    cmd = vec_get_at(u->commands, 0);
    ck_assert_ptr_nonnull(cmd);
    ck_assert_int_eq(*cmd, CT_Set);
}

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vmap");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_bind);
    tcase_add_test(tc_core, test_protected_mode);
    tcase_add_test(tc_core, test_port);
    tcase_add_test(tc_core, test_tcp_backlog);
    tcase_add_test(tc_core, test_loglevel);
    tcase_add_test(tc_core, test_logfile);
    tcase_add_test(tc_core, test_databases);
    tcase_add_test(tc_core, test_user);
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
