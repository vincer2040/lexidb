#include "parser.h"
#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void print_array_stmt(ArrayStatement* stmt);
void statement_free(Statement* stmt);
Statement parser_parse_statement(Parser* p);

int parser_debug = 1;

void parser_toggle_debug(int onoff) { parser_debug = onoff; }

#define parser_debug(...)                                                      \
    {                                                                          \
        if (parser_debug) {                                                    \
            printf("%s:%d\t", __FILE__, __LINE__);                             \
            printf(__VA_ARGS__);                                               \
            fflush(stdout);                                                    \
        }                                                                      \
    }

void print_bulk_stmt(BulkStatement* bulk_stmt) {
    if (parser_debug) {
        size_t i, len;
        len = bulk_stmt->len;
        printf("bulk (len: %lu): ", len);
        for (i = 0; i < len; ++i) {
            printf("%c", bulk_stmt->str[i]);
        }
        printf("\n");
    }
}

void print_statement(Statement* stmt) {
    if (stmt->type == SARR) {
        print_array_stmt(&(stmt->statement.arr));
    } else if (stmt->type == SBULK) {
        print_bulk_stmt(&(stmt->statement.bulk));
    }
}

void print_array_stmt(ArrayStatement* stmt) {
    size_t i, len;
    len = stmt->len;
    printf("array (len: %lu):\n", len);
    for (i = 0; i < len; ++i) {
        Statement s = stmt->statements[i];
        printf("\t");
        print_statement(&s);
    }
}

void print_cmd_ir(CmdIR* cmd_ir) {
    Statement stmt = cmd_ir->stmt;
    print_statement(&stmt);
}

void parser_next_token(Parser* p) {
    p->cur_tok = p->peek_tok;
    p->peek_tok = lexer_next_token(&(p->l));
}

uint8_t parser_cur_token_is(Parser* p, TokenT type) {
    return p->cur_tok.type == type ? 1 : 0;
}

uint8_t parser_peek_token_is(Parser* p, TokenT type) {
    return p->peek_tok.type == type ? 1 : 0;
}

uint8_t parser_expect_peek(Parser* p, TokenT type) {
    if (parser_peek_token_is(p, type)) {
        parser_next_token(p);
        return 1;
    }
    return 0;
}

ssize_t _parser_parse_len(Parser* p) {
    uint8_t* lit = p->cur_tok.literal;
    ssize_t len = 0;
    size_t i;
    for (i = 0; isdigit(lit[i]) != 0; i++) {
        len = (len * 10) + (lit[i] - '0');
    }
    return len;
}

/* we expext <length>\r\n */
ssize_t parser_parse_len(Parser* p) {
    ssize_t len;
    if (!parser_expect_peek(p, LEN)) {
        parser_debug("len is not after array declaration\n");
        return -1;
    }

    len = _parser_parse_len(p);

    if (!parser_expect_peek(p, RETCAR)) {
        parser_debug("expected retchar after len\n");
        return -1;
    }

    if (!parser_expect_peek(p, NEWL)) {
        parser_debug("expected new line after retchar\n");
        return -1;
    }

    parser_next_token(p);
    return len;
}

/* expect $<length>\r\n<string>\r\n */
BulkStatement parser_parse_bulk(Parser* p) {
    BulkStatement bulk_stmt = {0};
    ssize_t slen;

    slen = parser_parse_len(p);
    if (slen == -1) {
        return bulk_stmt;
    }

    bulk_stmt.len = ((size_t)slen);
    bulk_stmt.str = p->cur_tok.literal;
    if (!parser_expect_peek(p, RETCAR)) {
        parser_debug("expected retcar after bulk\n");
        memset(&bulk_stmt, 0, sizeof bulk_stmt);
        return bulk_stmt;
    }
    if (!parser_expect_peek(p, NEWL)) {
        parser_debug("expected retcar after retcar\n");
        memset(&bulk_stmt, 0, sizeof bulk_stmt);
        return bulk_stmt;
    }
    parser_next_token(p);
    return bulk_stmt;
}

/* expect *<length>\r\n<array> */
ArrayStatement parser_parse_array(Parser* p) {
    ArrayStatement array_stmt = {0};
    ssize_t slen;
    size_t i, len;

    slen = parser_parse_len(p);
    if (slen == -1) {
        return array_stmt;
    }

    len = ((size_t)slen);
    array_stmt.len = len;
    array_stmt.statements = calloc(len, sizeof(struct Statement));
    if (array_stmt.statements == NULL) {
        array_stmt.len = 0;
        return array_stmt;
    }

    for (i = 0; i < len; ++i) {
        array_stmt.statements[i] = parser_parse_statement(p);
    }

    return array_stmt;
}

StatementType parser_parse_statement_type(uint8_t* literal) {
    uint8_t type_byte = *literal;
    switch (type_byte) {
    case ((uint8_t)'*'):
        return SARR;
    case ((uint8_t)'$'):
        return SBULK;
    default:
        return SINVALID;
    }
}

Statement parser_parse_statement(Parser* p) {
    Statement stmt = {0};
    if (parser_cur_token_is(p, TYPE)) {
        stmt.type = parser_parse_statement_type(p->cur_tok.literal);
        switch (stmt.type) {
        case SARR:
            stmt.statement.arr = parser_parse_array(p);
            break;
        case SBULK:
            stmt.statement.bulk = parser_parse_bulk(p);
            break;
        default:
            parser_debug("invalid\n");
            stmt.type = SINVALID;
            break;
        }
        return stmt;
    } else {
        /* we should error here */
        print_tok(p->cur_tok);
    }
    return stmt;
}

CmdIR parse_cmd(Parser* p) {
    CmdIR cmd_ir = {0};
    cmd_ir.stmt = parser_parse_statement(p);
    // if (cmd_ir.statements == NULL) {
    //     return cmd_ir;
    // }
    // for (; p->cur_tok.type != EOFT; ++len) {
    //     if (len == cap) {
    //         cap += cap;
    //         cmd_ir.statements =
    //             realloc(cmd_ir.statements, sizeof(Statement) * cap);
    //         memset(cmd_ir.statements + len, 0, sizeof(Statement) * (cap - len));
    //     }
    //     cmd_ir.statements[len] = parser_parse_statement(p);
    // }

    // cmd_ir.len = len;

    return cmd_ir;
}

Parser parser_new(Lexer* l) {
    Parser p = {0};

    p.l = *l;

    parser_next_token(&p);
    parser_next_token(&p);

    return p;
}

void array_stmt_free(ArrayStatement* stmt) {
    size_t i, len;
    len = stmt->len;
    for (i = 0; i < len; ++i) {
        Statement s = stmt->statements[i];
        statement_free(&s);
    }
    free(stmt->statements);
}

void statement_free(Statement* stmt) {
    StatementType type = stmt->type;
    if (type == SARR) {
        ArrayStatement array_stmt = stmt->statement.arr;
        array_stmt_free(&array_stmt);
    }
}

void cmdir_free(CmdIR* cmd_ir) {
    Statement stmt = cmd_ir->stmt;
    statement_free(&stmt);
}
