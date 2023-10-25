#ifndef __PARSER_H__

#define __PARSER_H__

#include "lexer.h"
#include "token.h"

typedef struct {
    TokenT exp;
    Token got;
} ParserError;

typedef struct {
    size_t len;
    size_t cap;
    ParserError* errs;
} ParserErrors;

typedef struct {
    Lexer l;
    Token cur_tok;
    Token peek_tok;
    ParserErrors errors;
} Parser;

typedef enum {
    SINVALID,
    SARR,
    SBULK,
    SINT,
    SPING,
    SOK,
} StatementType;

struct Statement;

typedef enum {
    SST_INVALID,
    SST_PING,
    SST_OK,
} SimpleStringStatement;

/* this does not containt an allocated string, just a ptr at the position
 * of the start of this str in the input
 */
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
    SimpleStringStatement sst;
    int64_t i64;
} _Statement;

typedef struct Statement {
    StatementType type;
    _Statement statement;
} Statement;

typedef struct {
    Statement stmt;
} CmdIR;

Parser parser_new(Lexer* l);
CmdIR parse_cmd(Parser* p);
void print_cmd_ir(CmdIR* cmd_ir);
void cmdir_free(CmdIR* cmd_ir);
void parser_free_errors(Parser* p);
size_t parser_errors_len(Parser* p);

void parser_toggle_debug(int onoff);

#endif
