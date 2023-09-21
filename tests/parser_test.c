#include "../src/cmd.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/token.h"
#include "../src/builder.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <check.h>

START_TEST(test_it_works) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    ArrayStatement astmt;

    uint8_t* input =
        ((uint8_t*)"*2\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    assert(cmd_ir.stmt.type == SARR);

    astmt = cmd_ir.stmt.statement.arr;
    assert(astmt.len == 2);

    Statement stmt0 = astmt.statements[0];
    Cmd cmd0 = cmd_from_statement(&stmt0);
    assert(cmd0.type == SET);
    assert(cmd0.expression.set.key.len == 5);
    assert(cmd0.expression.set.value.size == 7);

    Statement stmt1 = astmt.statements[1];
    Cmd cmd1 = cmd_from_statement(&stmt1);
    assert(cmd1.type == GET);
    assert(cmd0.expression.get.key.len == 5);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_simple_string) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"*2\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_integers) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"*2\r*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_arr_len) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"*\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_arr_type) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"3\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_simple_ping) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    Cmd cmd;

    uint8_t* input = ((uint8_t*)"+PING\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    assert(cmd_ir.stmt.type == SPING);
    assert(cmd_ir.stmt.statement.sst == SST_PING);

    cmd = cmd_from_statement(&(cmd_ir.stmt));
    assert(cmd.type == CPING);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);

}
END_TEST

START_TEST(test_missing_str_type) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"*3\r\n3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis cool\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len > 0); // e_len should equal 3 for some reason

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_str_len) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    parser_toggle_debug(0);

    uint8_t* input =
        ((uint8_t*)"*3\r\n$\r\nSET\r\n$5\r\nvince\r\n$7\r\nis cool\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    assert(e_len > 0); // e_len = 3

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_integers_two) {
    Builder b = builder_create(32);
    size_t buf_len;
    uint8_t* buf;
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    Cmd cmd;
    int64_t v;
    builder_add_arr(&b, 3);
    builder_add_string(&b, "SET", 3);
    builder_add_string(&b, "vince", 5);
    builder_add_int(&b, 42069);

    buf_len = b.ins;
    buf = builder_out(&b);

    l = lexer_new(buf, buf_len);
    p = parser_new(&l);
    cmd_ir = parse_cmd(&p);

    assert(cmd_ir.stmt.type == SARR);

    cmd = cmd_from_statement(&(cmd_ir.stmt));
    assert(cmd.expression.set.value.type == VTINT);
    v = ((int64_t)(cmd.expression.set.value.ptr));
    assert(v == 42069);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

Suite* suite() {
    Suite* s;
    TCase* tc_core;
    s = suite_create("parser_test");
    tc_core = tcase_create("Core");
    tcase_add_test(tc_core, test_it_works);
    tcase_add_test(tc_core, test_simple_string);
    tcase_add_test(tc_core, test_integers);
    tcase_add_test(tc_core, test_missing_arr_len);
    tcase_add_test(tc_core, test_missing_arr_type);
    tcase_add_test(tc_core, test_simple_ping);
    tcase_add_test(tc_core, test_missing_str_type);
    tcase_add_test(tc_core, test_missing_str_len);
    tcase_add_test(tc_core, test_integers_two);
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

