#include "../src/vec.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>

#define arr_size(arr) sizeof arr / sizeof arr[0]

int cmp(const void* a, const void* b) {
    int a1 = *(int*)a;
    int b1 = *(int*)b;
    return a1 - b1;
}

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

START_TEST(test_vec_find) {
    vec* vec = vec_new(sizeof(int));
    int a1 = 5, a2 = 7, a3 = 9, a4 = 11;
    const int* res;
    ck_assert_int_eq(vec_push(&vec, &a1), 0);
    ck_assert_int_eq(vec_push(&vec, &a2), 0);
    ck_assert_int_eq(vec_push(&vec, &a3), 0);
    res = vec_find(vec, &a1, cmp);
    ck_assert_ptr_nonnull(res);
    res = vec_find(vec, &a2, cmp);
    ck_assert_ptr_nonnull(res);
    res = vec_find(vec, &a3, cmp);
    ck_assert_ptr_nonnull(res);
    res = vec_find(vec, &a4, cmp);
    ck_assert_ptr_null(res);
    vec_free(vec, NULL);
}
END_TEST

START_TEST(test_vec_iter) {
    int vars[] = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10
    };
    size_t i, len = arr_size(vars);
    vec* v = vec_new(sizeof(int));
    vec_iter iter;
    for (i = 0; i < len; ++i) {
        int c = vars[i];
        vec_push(&v, &c);
    }
    iter = vec_iter_new(v);
    for (i = 0; i < len; ++i) {
        const int* cur = iter.cur;
        int t = vars[i];
        ck_assert_ptr_nonnull(cur);

        ck_assert_int_eq(*cur, t);
        vec_iter_next(&iter);
    }
    ck_assert_ptr_null(iter.cur);
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("vec");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_vec_find);
    tcase_add_test(tc_core, test_vec_iter);
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
