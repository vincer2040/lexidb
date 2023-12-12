#include "../src/ht.h"
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

START_TEST(test_it_works) {
    ht ht = ht_new(sizeof(int), NULL);

    int a0 = 0, a1 = 1, a2 = 2, a3 = 3, a4 = 4, a5 = 5;

    int* get;
    ck_assert_int_eq(ht_insert(&ht, "a0", 2, &a0, NULL), 0);
    ck_assert_int_eq(ht_insert(&ht, "a1", 2, &a1, NULL), 0);
    ck_assert_int_eq(ht_insert(&ht, "a2", 2, &a2, NULL), 0);
    ck_assert_int_eq(ht_insert(&ht, "a3", 2, &a3, NULL), 0);
    ck_assert_int_eq(ht_insert(&ht, "a4", 2, &a4, NULL), 0);
    ck_assert_int_eq(ht_insert(&ht, "a5", 2, &a5, NULL), 0);

    ck_assert_uint_eq(ht.num_entries, 6);

    get = ht_get(&ht, "a0", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a0);

    get = ht_get(&ht, "a1", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a1);

    get = ht_get(&ht, "a2", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a2);

    get = ht_get(&ht, "a3", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a3);

    get = ht_get(&ht, "a4", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a4);

    get = ht_get(&ht, "a5", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a5);

    get = ht_get(&ht, "a6", 2);
    ck_assert_ptr_null(get);

    ck_assert_int_eq(ht_delete(&ht, "a0", 2, NULL, NULL), 0);
    ck_assert_uint_ge(ht.num_entries, 5);
    ck_assert_int_eq(ht_delete(&ht, "a6", 2, NULL, NULL), -1);

    get = ht_get(&ht, "a0", 2);
    ck_assert_ptr_null(get);

    get = ht_get(&ht, "a1", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a1);

    get = ht_get(&ht, "a2", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a2);

    get = ht_get(&ht, "a3", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a3);

    get = ht_get(&ht, "a4", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a4);

    get = ht_get(&ht, "a5", 2);
    ck_assert_ptr_nonnull(get);
    ck_assert_int_eq(*get, a5);

    get = ht_get(&ht, "a6", 2);
    ck_assert_ptr_null(get);

    ht_free(&ht, NULL, NULL);
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("ht");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
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
