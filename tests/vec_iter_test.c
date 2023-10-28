#include "../src/vec.h"
#include <check.h>
#include <stdlib.h>
#include <stdio.h>

START_TEST(test_it_works) {
    Vec* vec = vec_new(32, sizeof(int));
    int a0 = 1, a1 = 2, a2 = 3, a3 = 4;
    int exp[] = { a0, a1, a2, a3 };
    size_t i, len = sizeof(exp) / sizeof(exp[0]);
    int* cur;
    VecIter iter;
    vec_push(&vec, &a0);
    vec_push(&vec, &a1);
    vec_push(&vec, &a2);
    vec_push(&vec, &a3);

    iter = vec_iter_new(vec, 0);

    for (i = 0; i < len; ++i) {
        int exp_cur = exp[i];
        cur = iter.cur;
        ck_assert_ptr_nonnull(cur);

        ck_assert_int_eq(*cur, exp_cur);
        printf("%d\n", *cur);
        vec_iter_next(&iter);
    }

    vec_iter_next(&iter);
    cur = iter.cur;
    ck_assert_ptr_null(cur);

    vec_free(vec, NULL);
}
END_TEST

START_TEST(test_reverse) {
    Vec* vec = vec_new(32, sizeof(int));
    int a0 = 1, a1 = 2, a2 = 3, a3 = 4;
    int exp[] = { a3, a2, a1, a0 };
    size_t i, len = sizeof(exp) / sizeof(exp[0]);
    int* cur;
    VecIter iter;
    vec_push(&vec, &a0);
    vec_push(&vec, &a1);
    vec_push(&vec, &a2);
    vec_push(&vec, &a3);

    iter = vec_iter_new(vec, VEC_ITER_REVERSE);

    for (i = 0; i < len; ++i) {
        int exp_cur = exp[i];
        cur = iter.cur;
        ck_assert_ptr_nonnull(cur);

        ck_assert_int_eq(*cur, exp_cur);
        vec_iter_next(&iter);
    }

    vec_iter_next(&iter);
    cur = iter.cur;
    ck_assert_ptr_null(cur);

    vec_free(vec, NULL);
}
END_TEST

START_TEST(test_vec_with_one_element) {
    Vec* vec = vec_new(32, sizeof(int));
    int a0 = 42069;
    int exp[] = { a0 };
    int* cur;
    size_t i, len = sizeof(exp) / sizeof(exp[0]);
    VecIter iter;

    vec_push(&vec, &a0);

    iter = vec_iter_new(vec, 0);

    for (i = 0; i < len; ++i) {
        int exp_cur = exp[0];
        cur = iter.cur;
        ck_assert_ptr_nonnull(cur);

        ck_assert_int_eq(exp_cur, *cur);
        vec_iter_next(&iter);
    }

    vec_iter_next(&iter);
    cur = iter.cur;
    ck_assert_ptr_null(cur);

    vec_free(vec, NULL);
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vec iter test");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_reverse);
    tcase_add_test(tc_core, test_vec_with_one_element);
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

