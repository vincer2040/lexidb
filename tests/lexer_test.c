#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

START_TEST(test_it_works) {
    ck_assert(1 == 1);
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("lexer_test");
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
