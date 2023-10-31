#include "../src/builder.h"
#include "../src/cmd.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/token.h"
#include <assert.h>
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void assert_array_len(Statement* stmt, size_t len) {
    ck_assert_uint_eq(stmt->type, SARR);
    ck_assert_uint_eq(stmt->statement.arr.len, len);
}

void assert_bulk(Statement* stmt, char* exp, size_t len) {
    ck_assert_uint_eq(stmt->type, SBULK);
    ck_assert_mem_eq(stmt->statement.bulk.str, exp, len);
}

static void assert_replication_ht(Cmd* cmd) {
    size_t i, len;
    ck_assert_uint_eq(cmd->type, HT);
    ck_assert_uint_eq(cmd->expression.multi.len, 3);
    len = cmd->expression.multi.len;

    for (i = 0; i < len; ++i) {
        Cmd cur = cmd->expression.multi.commands[i];
        ck_assert_uint_eq(cur.type, SET);
    }
}

static void assert_replication_stack(Cmd* cmd) {
    size_t i, len;
    ck_assert_uint_eq(cmd->type, STACK);
    ck_assert_uint_eq(cmd->expression.multi.len, 2);
    len = cmd->expression.multi.len;

    for (i = 0; i < len; ++i) {
        Cmd cur = cmd->expression.multi.commands[i];
        ck_assert_uint_eq(cur.type, PUSH);
    }
}

static void assert_replication_queue(Cmd* cmd) {
    size_t i, len;
    ck_assert_uint_eq(cmd->type, QUEUE);
    ck_assert_uint_eq(cmd->expression.multi.len, 1);
    len = cmd->expression.multi.len;

    for (i = 0; i < len; ++i) {
        Cmd cur = cmd->expression.multi.commands[i];
        ck_assert_uint_eq(cur.type, ENQUE);
    }
}

static void assert_replication_cluster(Cmd* cmd) {
    size_t i, len;
    ck_assert_uint_eq(cmd->type, CLUSTER);
    ck_assert_uint_eq(cmd->expression.multi.len, 2);

    len = cmd->expression.multi.len;

    for (i = 0; i < len; ++i) {
        Cmd cur = cmd->expression.multi.commands[i];
        ck_assert_uint_eq(cur.type, MULTI_CMD);
        ck_assert_uint_eq(cur.expression.multi.len, 2);
    }
}

void create_replication_buf(Builder* b) {
    builder_add_arr(b, 2);
    builder_add_string(b, "REPLICATION", 11);
    builder_add_arr(b, 4);

    builder_add_arr(b, 2);
    builder_add_string(b, "HT", 2);
    builder_add_arr(b, 3);
    builder_add_arr(b, 2);
    builder_add_string(b, "vince", 5);
    builder_add_string(b, "is cool", 7);
    builder_add_arr(b, 2);
    builder_add_string(b, "vince1", 6);
    builder_add_string(b, "is cool", 7);
    builder_add_arr(b, 2);
    builder_add_string(b, "vince2", 6);
    builder_add_string(b, "is cool", 7);

    builder_add_arr(b, 2);
    builder_add_string(b, "STACK", 5);
    builder_add_arr(b, 2);
    builder_add_string(b, "foo", 3);
    builder_add_string(b, "bar", 3);

    builder_add_arr(b, 2);
    builder_add_string(b, "QUEUE", 5);
    builder_add_arr(b, 1);
    builder_add_string(b, "baz", 3);

    builder_add_arr(b, 2);
    builder_add_string(b, "CLUSTER", 7);

    builder_add_arr(b, 2);

    builder_add_arr(b, 3);
    builder_add_string(b, "fam", 3);
    builder_add_arr(b, 2);
    builder_add_string(b, "HT", 2);
    builder_add_arr(b, 1);
    builder_add_arr(b, 2);
    builder_add_string(b, "vince", 5);
    builder_add_string(b, "is cool", 7);
    builder_add_arr(b, 2);
    builder_add_string(b, "STACK", 5);
    builder_add_arr(b, 0);

    builder_add_arr(b, 3);
    builder_add_string(b, "fam1", 4);
    builder_add_arr(b, 2);
    builder_add_string(b, "HT", 2);
    builder_add_arr(b, 0);
    builder_add_arr(b, 2);
    builder_add_string(b, "STACK", 5);
    builder_add_arr(b, 1);
    builder_add_string(b, "foobar", 6);
}

START_TEST(test_it_works) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    ArrayStatement astmt;

    uint8_t* input =
        ((uint8_t*)"*2\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    assert(cmd_ir.stmt.type == SARR);

    astmt = cmd_ir.stmt.statement.arr;
    assert(astmt.len == 2);

    Statement stmt0 = astmt.statements[0];
    Cmd cmd0 = cmd_from_statement(&stmt0);
    ck_assert(cmd0.type == SET);
    ck_assert(cmd0.expression.set.key.len == 5);
    ck_assert(cmd0.expression.set.value.size == 7);

    Statement stmt1 = astmt.statements[1];
    Cmd cmd1 = cmd_from_statement(&stmt1);
    ck_assert(cmd1.type == GET);
    ck_assert(cmd0.expression.get.key.len == 5);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_simple_string) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"*2\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_integers) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"*2\r*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_arr_len) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"*\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len == 1);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_arr_type) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"3\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis "
                   "cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len == 1);

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

    ck_assert(cmd_ir.stmt.type == SPING);
    ck_assert(cmd_ir.stmt.statement.sst == SST_PING);

    cmd = cmd_from_statement(&(cmd_ir.stmt));
    ck_assert(cmd.type == CPING);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_str_type) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"*3\r\n3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis cool\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len > 0); // e_len should equal 3 for some reason

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_missing_str_len) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    size_t e_len;

    uint8_t* input =
        ((uint8_t*)"*3\r\n$\r\nSET\r\n$5\r\nvince\r\n$7\r\nis cool\r\n");
    size_t inp_len = strlen(((char*)input));

    parser_toggle_debug(0);
    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);

    e_len = parser_errors_len(&p);

    ck_assert(e_len > 0); // e_len = 3

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
    parser_toggle_debug(0);
    builder_add_arr(&b, 3);
    builder_add_string(&b, "SET", 3);
    builder_add_string(&b, "vince", 5);
    builder_add_int(&b, 42069);

    buf_len = b.ins;
    buf = builder_out(&b);

    l = lexer_new(buf, buf_len);
    p = parser_new(&l);
    cmd_ir = parse_cmd(&p);

    ck_assert(cmd_ir.stmt.type == SARR);

    cmd = cmd_from_statement(&(cmd_ir.stmt));
    ck_assert(cmd.expression.set.value.type == VTINT);
    v = ((int64_t)(cmd.expression.set.value.ptr));
    ck_assert(v == 42069);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_negative_integers) {
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
    builder_add_int(&b, -20);

    buf_len = b.ins;
    buf = builder_out(&b);

    parser_toggle_debug(0);
    l = lexer_new(buf, buf_len);
    p = parser_new(&l);
    cmd_ir = parse_cmd(&p);

    ck_assert(cmd_ir.stmt.type == SARR);

    cmd = cmd_from_statement(&(cmd_ir.stmt));
    ck_assert(cmd.expression.set.value.type == VTINT);
    v = ((int64_t)(cmd.expression.set.value.ptr));
    ck_assert(v == -20);

    cmdir_free(&cmd_ir);
    parser_free_errors(&p);
}
END_TEST

START_TEST(test_array_zero_len) {
    uint8_t* input = (uint8_t*)"*0\r\n";
    parser_toggle_debug(0);
    Lexer l = lexer_new(input, strlen((char*)input));
    Parser p = parser_new(&l);
    CmdIR ir = parse_cmd(&p);
    ck_assert_uint_eq(ir.stmt.type, SARR);
    ck_assert_uint_eq(ir.stmt.statement.arr.len, 0);
    cmdir_free(&ir);
}
END_TEST

START_TEST(test_ok) {
    uint8_t* buf = (uint8_t*)"+OK\r\n";
    parser_toggle_debug(0);
    Lexer l = lexer_new(buf, strlen((char*)buf));
    Parser p = parser_new(&l);
    CmdIR ir = parse_cmd(&p);
    ck_assert_uint_eq(ir.stmt.type, SOK);
    cmdir_free(&ir);
}
END_TEST

START_TEST(test_replication_from_master) {
    Builder b = builder_create(32);
    Lexer l;
    Parser p;
    CmdIR cmd_ir;
    Cmd cmd, ht_cmd, stack_cmd, queue_cmd, cluster_cmd;

    create_replication_buf(&b);

    parser_toggle_debug(0);
    l = lexer_new(builder_out(&b), b.ins);
    p = parser_new(&l);
    cmd_ir = parse_cmd(&p);
    cmd = cmd_from_statement(&(cmd_ir.stmt));

    ck_assert_uint_eq(cmd.type, REPLICATION);
    ck_assert_uint_eq(cmd.expression.multi.len, 4);

    ht_cmd = cmd.expression.multi.commands[0];
    assert_replication_ht(&ht_cmd);
    stack_cmd = cmd.expression.multi.commands[1];
    assert_replication_stack(&stack_cmd);
    queue_cmd = cmd.expression.multi.commands[2];
    assert_replication_queue(&queue_cmd);
    cluster_cmd = cmd.expression.multi.commands[3];
    assert_replication_cluster(&cluster_cmd);

    builder_free(&b);
    cmdir_free(&cmd_ir);
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
    tcase_add_test(tc_core, test_negative_integers);
    tcase_add_test(tc_core, test_array_zero_len);
    tcase_add_test(tc_core, test_ok);
    tcase_add_test(tc_core, test_replication_from_master);
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
