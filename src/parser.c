#include "parser.h"
#include "lexer.h"
#include "token.h"
#include "util.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void print_array_stmt(ArrayStatement* stmt);
void statement_free(Statement* stmt);
Statement parser_parse_statement(Parser* p);

static int parser_debug = 1;

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

ParserError parser_new_err(TokenT exp, Token* got) {
    ParserError pe = {0};

    pe.exp = exp;
    pe.got = *got;
    return pe;
}

void parser_append_error(Parser* p, ParserError* e) {
    size_t len = p->errors.len;
    size_t cap = p->errors.cap;
    if (len == cap) {
        cap += cap;
        p->errors.errs = realloc(p->errors.errs, sizeof(ParserError) * cap);
        if (p->errors.errs == NULL) {
            fmt_error("out of memory\n");
            p->errors.cap = 0;
            p->errors.len = 0;
            return;
        }

        memset(p->errors.errs + len, 0, sizeof(ParserError) * (cap - len));
        p->errors.cap = cap;
    }

    p->errors.errs[len] = *e;
    p->errors.len += 1;
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
        ParserError e = parser_new_err(LEN, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("len is not after array declaration\n");
        return -1;
    }

    len = _parser_parse_len(p);

    if (!parser_expect_peek(p, RETCAR)) {
        ParserError e = parser_new_err(RETCAR, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected retchar after len\n");
        return -1;
    }

    if (!parser_expect_peek(p, NEWL)) {
        ParserError e = parser_new_err(NEWL, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected new line after retchar\n");
        return -1;
    }

    parser_next_token(p);
    return len;
}

/**
 * expect $<length>\r\n<string>\r\n
 * length must be greater than 0
 */
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
        ParserError e = parser_new_err(RETCAR, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected retcar after bulk\n");
        memset(&bulk_stmt, 0, sizeof bulk_stmt);
        return bulk_stmt;
    }
    if (!parser_expect_peek(p, NEWL)) {
        ParserError e = parser_new_err(NEWL, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected newl after retcar\n");
        memset(&bulk_stmt, 0, sizeof bulk_stmt);
        return bulk_stmt;
    }
    parser_next_token(p);
    return bulk_stmt;
}

/**
 * expect *<length>\r\n<array>
 * length must be greater than 0
 */
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
    array_stmt.statements = calloc(len, sizeof(Statement));
    if (array_stmt.statements == NULL) {
        array_stmt.len = 0;
        return array_stmt;
    }

    for (i = 0; i < len; ++i) {
        array_stmt.statements[i] = parser_parse_statement(p);
    }

    return array_stmt;
}

/**
 * Simple strings must be the only command sent
 * and cannot be sent in an array, so we expect EOFT at peek
 *
 * Should this be the case ? probably not. It should just
 * work if this check gets removed though
 */
SimpleStringStatement parser_parse_simple_string(Parser* p) {
    if (!parser_expect_peek(p, EOFT)) {
        ParserError e = parser_new_err(EOFT, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected EOF after simple\n");
        return SST_INVALID;
    }
    return SST_PING;
}

/**
 * the byte order is not of our concern right now - we
 * simply unpack whatever bytes were sent to us into an
 * int64_t
 */
int64_t parser_parse_int(Parser* p) {
    int64_t res = 0;
    uint64_t temp = 0;
    uint8_t* buf = p->cur_tok.literal;
    size_t i;
    size_t len = 9;
    uint8_t shift = 56;

    // maybe just replace this with memcpy ?
    for (i = 1; i < len; ++i, shift -= 8) {
        uint8_t at = buf[i];
        temp |= (uint64_t)(at) << shift;
    }

    // hack for checking if value should be negative
    if (temp <= 0x7fffffffffffffffu) {
        res = temp;
    } else {
        res = -1 - (long long int)(0xffffffffffffffffu - temp);
    }

    // todo: some sort of error handling...
    if (!parser_expect_peek(p, RETCAR)) {
        ParserError e = parser_new_err(RETCAR, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected retcar after int\n");
        return -1;
    }
    if (!parser_expect_peek(p, NEWL)) {
        ParserError e = parser_new_err(NEWL, &(p->peek_tok));
        parser_append_error(p, &e);
        parser_debug("expected newl after retcar\n");
        return -1;
    }
    parser_next_token(p);
    return res;
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
        ParserError e;
        stmt.type = parser_parse_statement_type(p->cur_tok.literal);
        switch (stmt.type) {
        case SARR:
            stmt.statement.arr = parser_parse_array(p);
            break;
        case SBULK:
            stmt.statement.bulk = parser_parse_bulk(p);
            break;
        default:
            e = parser_new_err(TYPE, &(p->cur_tok));
            parser_append_error(p, &e);
            parser_debug("syntax error, invalid type\n");
            stmt.type = SINVALID;
            break;
        }
        return stmt;
    }
    if (parser_cur_token_is(p, PING)) {
        stmt.type = SPING;
        stmt.statement.sst = parser_parse_simple_string(p);
        return stmt;
    }
    if (parser_cur_token_is(p, INT)) {
        stmt.type = SINT;
        stmt.statement.i64 = parser_parse_int(p);
        return stmt;
    }
    ParserError e = parser_new_err(TYPE, &(p->cur_tok));
    parser_append_error(p, &e);
    parser_debug("syntax error, no type\n");
    return stmt;
}

ParserErrors parser_init_errors(size_t initial_cap) {
    ParserErrors errors = {0};

    errors.errs = calloc(initial_cap, sizeof(ParserError));

    if (errors.errs == NULL) {
        return errors;
    }

    errors.cap = initial_cap;

    return errors;
}

size_t parser_errors_len(Parser* p) { return p->errors.len; }

CmdIR parse_cmd(Parser* p) {
    CmdIR cmd_ir = {0};
    cmd_ir.stmt = parser_parse_statement(p);
    return cmd_ir;
}

Parser parser_new(Lexer* l) {
    Parser p = {0};

    p.l = *l;

    parser_next_token(&p);
    parser_next_token(&p);

    p.errors = parser_init_errors(5);

    return p;
}

void parser_free_errors(Parser* p) { free(p->errors.errs); }

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
