#include "../src/parser.h"
#include "../src/lexer.h"
#include "../src/token.h"
#include <string.h>

int main(void) {
    Lexer l;
    Parser p;
    CmdIR cmd_ir;

    uint8_t* input = ((uint8_t*)"*2\r\n*3\r\n$3\r\nSET\r\n$5\r\nvince\r\n$7\r\nis cool\r\n*2\r\n$3\r\nGET\r\n$5\r\nvince\r\n");
    size_t inp_len = strlen(((char*)input));

    l = lexer_new(input, inp_len);
    p = parser_new(&l);

    cmd_ir = parse_cmd(&p);
    print_cmd_ir(&cmd_ir);

    cmdir_free(&cmd_ir);

    return 0;
}
