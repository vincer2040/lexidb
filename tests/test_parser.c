#include "../src/cmd.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/token.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void t1() {
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
    printf("parser test 1 passed\n");
}

int main(void) {
    t1();
    printf("all parser tests passed\n");
    return 0;
}
