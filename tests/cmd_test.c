#include "../src/cmd.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/builder.h"
#include <check.h>
#include <stdlib.h>
#include <stdio.h>

START_TEST(test_multi_commands) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    Builder b;
    uint8_t* buf;
    size_t buf_len;
    b = builder_create(32);
    builder_add_arr(&b, 2);
    builder_add_arr(&b, 3);
    builder_add_string(&b, "SET", 3);
    builder_add_string(&b, "vince", 5);
    builder_add_string(&b, "is cool", 7);
    builder_add_arr(&b, 2);
    builder_add_string(&b, "GET", 3);
    builder_add_string(&b, "vince", 5);
    buf_len = b.ins;
    buf = builder_out(&b);

    l = lexer_new(buf, buf_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    ck_assert_uint_eq(cmd_ir.stmt.type, SARR);
    ck_assert_uint_eq(cmd_ir.stmt.statement.arr.len, 2);

    Statement stmt_set = cmd_ir.stmt.statement.arr.statements[0];
    ck_assert_uint_eq(stmt_set.type, SARR);
    ck_assert_uint_eq(stmt_set.statement.arr.len, 3);

    Statement stmt_get = cmd_ir.stmt.statement.arr.statements[1];
    ck_assert_uint_eq(stmt_get.type, SARR);
    ck_assert_uint_eq(stmt_get.statement.arr.len, 2);

    free(stmt_set.statement.arr.statements);
    free(stmt_get.statement.arr.statements);
    free(cmd_ir.stmt.statement.arr.statements);
    free(buf);
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("cmd test");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_multi_commands);
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

