#ifndef __PARSER_H__

#define __PARSER_H__

#include "lexer.h"
#include "token.h"

typedef struct {
    Lexer l;
    Token cur_tok;
    Token peek_tok;
} Parser;

typedef enum { SINVALID, SARR, SBULK } StatementType;

struct Statement;

typedef struct {
    size_t len;
    uint8_t* str;
} BulkStatement;

typedef struct {
    size_t len;
    struct Statement* statements;
} ArrayStatement;

typedef union {
    BulkStatement bulk;
    ArrayStatement arr;
} _Statement;

typedef struct Statement {
    StatementType type;
    _Statement statement;
} Statement;

typedef struct {
    size_t len;
    size_t cap;
    Statement* statements;
} CmdIR;

Parser parser_new(Lexer* l);
CmdIR parse_cmd(Parser* p);
void print_cmd_ir(CmdIR* cmd_ir);
void cmdir_free(CmdIR* cmd_ir);

#endif
