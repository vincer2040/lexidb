#include "../src/trie.h"
#include <check.h>
#include <stdlib.h>

#define arr_size(arr) sizeof arr / sizeof arr[0]

START_TEST(test_it_works) {
    trie* trie = trie_new();

    ck_assert_int_eq(trie_insert(trie, "foo", 3), TRIE_OK);
    ck_assert_int_eq(trie_insert(trie, "bar", 3), TRIE_OK);
    ck_assert_int_eq(trie_insert(trie, "food", 4), TRIE_OK);

    ck_assert_uint_eq(trie_numel(trie), 3);

    ck_assert_int_eq(trie_search(trie, "foo", 3), TRIE_OK);
    ck_assert_int_eq(trie_search(trie, "bar", 3), TRIE_OK);
    ck_assert_int_eq(trie_search(trie, "food", 4), TRIE_OK);
    ck_assert_int_eq(trie_search(trie, "foot", 4), TRIE_NO_KEY);

    ck_assert_int_eq(trie_insert(trie, "foo", 3), TRIE_KEY_ALREADY_EXISTS);

    trie_free(trie);
}
END_TEST

START_TEST(test_score) {
    trie* trie = trie_new();
    const char* inserts[] = {
        "foo",
        "bar",
        "foobar",
        "barbaz",
    };
    size_t i, len = arr_size(inserts);
    for (i = 0; i < len; ++i) {
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        ck_assert_int_eq(trie_insert(trie, ins, ins_len), TRIE_OK);
    }
    for (i = 0; i < len; ++i) {
        int res = 0;
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        uint32_t score = trie_get_score(trie, ins, ins_len, &res);
        ck_assert_int_eq(res, TRIE_OK);
        ck_assert_uint_eq(score, 0);
    }
    for (i = 0; i < len; ++i) {
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        ck_assert_int_eq(trie_set_score(trie, ins, ins_len, i), TRIE_OK);
    }
    for (i = 0; i < len; ++i) {
        int res = 0;
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        uint32_t score = trie_get_score(trie, ins, ins_len, &res);
        ck_assert_int_eq(res, TRIE_OK);
        ck_assert_uint_eq(score, i);
    }
    for (i = 0; i < len; ++i) {
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        ck_assert_int_eq(trie_incr_score(trie, ins, ins_len), TRIE_OK);
    }
    for (i = 0; i < len; ++i) {
        int res = 0;
        const char* ins = inserts[i];
        size_t ins_len = strlen(ins);
        uint32_t score = trie_get_score(trie, ins, ins_len, &res);
        ck_assert_int_eq(res, TRIE_OK);
        ck_assert_uint_eq(score, i + 1);
    }
    trie_free(trie);
}
END_TEST

Suite* suite(void) {
    Suite* s;
    TCase* tc_core;
    s = suite_create("trie");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_score);
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
