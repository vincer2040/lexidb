#include "../src/vec.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>

START_TEST(test_it_works) {
    vec* vec = vec_new(sizeof(int));
    int a1 = 5, a2 = 7, a3 = 9, a4 = 11;
    int out;
    ck_assert_int_eq(vec_push(&vec, &a1), 0);
    ck_assert_int_eq(vec_push(&vec, &a2), 0);
    ck_assert_int_eq(vec_push(&vec, &a3), 0);

    ck_assert_int_eq(vec_pop(vec, &out), 0);
    ck_assert_int_eq(out, 9);

    ck_assert_uint_eq(vec_len(vec), 2);

    ck_assert_int_eq(vec_push(&vec, &a4), 0);

    ck_assert_int_eq(vec_pop(vec, &out), 0);
    ck_assert_int_eq(out, 11);

    ck_assert_int_eq(vec_pop(vec, &out), 0);
    ck_assert_int_eq(out, 7);

    ck_assert_int_eq(vec_pop(vec, &out), 0);
    ck_assert_int_eq(out, 5);

    ck_assert_int_eq(vec_pop(vec, &out), -1);

    ck_assert_int_eq(vec_push(&vec, &a1), 0);
    ck_assert_uint_eq(vec_len(vec), 1);

    vec_free(vec, NULL);
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vec");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
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
