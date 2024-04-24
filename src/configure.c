#include "cmd.h"
#include "server.h"
#include "util.h"
#include "vstr.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char* input;
    size_t input_len;
    size_t pos;
    char ch;
    int has_error;
    vstr error;
} config_parser;

typedef enum {
    CO_Invalid,
    CO_Bind,
    CO_Protected_mode,
    CO_Port,
    CO_TCP_Backlog,
    CO_LogLevel,
    CO_LogFile,
    CO_Databases,
    CO_User,
} config_option;

typedef struct {
    const char* str;
    size_t str_len;
    config_option opt;
} config_option_lookup;

typedef struct {
    const char* str;
    size_t str_len;
    log_level level;
} loglevel_lookup;

typedef struct {
    const char* str;
    size_t str_len;
    category cat;
} category_lookup;

typedef struct {
    const char* str;
    size_t str_len;
    cmd_type cmd;
} cmd_lookup;

static void config_parser_configure_server(lexi_server* s, config_parser* p);
static void config_parser_parse_line(lexi_server* s, config_parser* p);
static config_option parse_config_option(config_parser* p);
static void config_parser_parse_bind(lexi_server* s, config_parser* p);
static void config_parser_parse_port(lexi_server* s, config_parser* p);
static void config_parser_parse_protected_mode(lexi_server* s, config_parser* p);
static void config_parser_parse_tcp_backlog(lexi_server* s, config_parser* p);
static void config_parser_parse_loglevel(lexi_server* s, config_parser* p);
static void config_parser_parse_logfile(lexi_server* s, config_parser* p);
static void config_parser_parse_num_databases(lexi_server* s, config_parser* p);
static void config_parser_parse_user(lexi_server* s, config_parser* p);
static category config_parser_parse_category(config_parser* p);
static cmd_type config_parser_parse_cmd_type(config_parser* p);
static vstr config_parser_read_basic_string(config_parser* p);
static vstr config_parser_read_number_string(config_parser* p);
static void config_parser_read_char(config_parser* p);
static char config_parser_peek(config_parser* p);
static void config_parser_skip_whitespace(config_parser* p);
static void config_parser_skip_comment(config_parser* p);
static vstr config_parser_parse_option(config_parser* p);

const config_option_lookup option_lookups[] = {
    {"user", 4, CO_User},
    {"bind", 4, CO_Bind},
    {"port", 4, CO_Port},
    {"logfile", 7, CO_LogFile},
    {"loglevel", 8, CO_LogLevel},
    {"databases", 9, CO_Databases},
    {"tcp-backlog", 11, CO_TCP_Backlog},
    {"protected-mode", 14, CO_Protected_mode},
};

size_t option_lookups_len = sizeof option_lookups / sizeof option_lookups[0];

const loglevel_lookup loglevel_lookups[] = {
    {"info", 4, LL_Info},       {"debug", 5, LL_Debug},
    {"nothing", 7, LL_Nothing}, {"warning", 7, LL_Warning},
    {"verbose", 7, LL_Verbose},
};

size_t loglevel_lookups_len =
    sizeof loglevel_lookups / sizeof loglevel_lookups[0];

const category_lookup category_lookups[] = {
    {"read", 4, C_Read},
    {"write", 5, C_Write},
    {"admin", 5, C_Admin},
    {"connection", 10, C_Connection},
};

size_t category_lookups_len =
    sizeof category_lookups / sizeof category_lookups[0];

const cmd_lookup cmd_lookups[] = {
    {"set", 3, CT_Set},
    {"get", 3, CT_Get},
    {"del", 3, CT_Del},
    {"info", 4, CT_Info},
};

size_t cmd_lookups_len = sizeof cmd_lookups / sizeof cmd_lookups[0];

static config_parser config_parser_new(const char* input, size_t input_len) {
    config_parser p = {0};
    p.input = input;
    p.input_len = input_len;
    config_parser_read_char(&p);
    return p;
}

int configure_server(lexi_server* s, const char* input, size_t input_len,
                     vstr* error) {
    config_parser p;
    if (s->users == NULL) {
        if (error) {
            *error = vstr_from("expected users vector to not be null");
        }
        return -1;
    }
    p = config_parser_new(input, input_len);
    config_parser_configure_server(s, &p);
    if (p.has_error) {
        if (error) {
            *error = p.error;
            return -1;
        }
    }
    return 0;
}

static void config_parser_configure_server(lexi_server* s, config_parser* p) {
    while (p->ch != 0) {
        config_parser_parse_line(s, p);
        if (p->has_error) {
            return;
        }
        config_parser_read_char(p);
    }
}

static void config_parser_parse_line(lexi_server* s, config_parser* p) {
    config_option opt;
    config_parser_skip_whitespace(p);
    if (p->ch == '#') {
        config_parser_skip_comment(p);
        return;
    }
    opt = parse_config_option(p);
    config_parser_read_char(p);
    switch (opt) {
    case CO_Invalid:
        return;
    case CO_Bind:
        config_parser_parse_bind(s, p);
        break;
    case CO_Port:
        config_parser_parse_port(s, p);
        break;
    case CO_Protected_mode:
        config_parser_parse_protected_mode(s, p);
        break;
    case CO_TCP_Backlog:
        config_parser_parse_tcp_backlog(s, p);
        break;
    case CO_LogLevel:
        config_parser_parse_loglevel(s, p);
        break;
    case CO_LogFile:
        config_parser_parse_logfile(s, p);
        break;
    case CO_Databases:
        config_parser_parse_num_databases(s, p);
        break;
    case CO_User:
        config_parser_parse_user(s, p);
        break;
    }
}

static config_option parse_config_option(config_parser* p) {
    vstr opt_vstr = vstr_new();
    const char* opt_str;
    size_t i, opt_str_len;
    while ((p->ch != ' ') && (p->ch != 0)) {
        vstr_push_char(&opt_vstr, p->ch);
        config_parser_read_char(p);
    }
    opt_str = vstr_data(&opt_vstr);
    opt_str_len = vstr_len(&opt_vstr);
    for (i = 0; i < option_lookups_len; ++i) {
        config_option_lookup l = option_lookups[i];
        if (opt_str_len != l.str_len) {
            continue;
        }
        if (strncmp(opt_str, l.str, opt_str_len) != 0) {
            continue;
        }
        vstr_free(&opt_vstr);
        return l.opt;
    }
    p->has_error = 1;
    p->error = vstr_format("unknown config option: %s", opt_str);
    vstr_free(&opt_vstr);
    return CO_Invalid;
}

static void config_parser_parse_bind(lexi_server* s, config_parser* p) {
    vstr addr = config_parser_read_basic_string(p);
    s->bind_addr = addr;
}

static void config_parser_parse_port(lexi_server* s, config_parser* p) {
    uint16_t port = 0;
    while (p->ch != '\n' && p->ch != 0) {
        port = (port * 10) + (p->ch - '0');
        config_parser_read_char(p);
    }
    s->port = port;
}

static void config_parser_parse_protected_mode(lexi_server* s, config_parser* p) {
    vstr str = vstr_new();
    const char* prot_str;
    size_t prot_str_len;
    while ((p->ch != '\n') && (p->ch != 0)) {
        vstr_push_char(&str, p->ch);
        config_parser_read_char(p);
    }
    prot_str = vstr_data(&str);
    prot_str_len = vstr_len(&str);
    if ((prot_str_len == 3) && (strncmp(prot_str, "yes", 3) == 0)) {
        s->protected_mode = 1;
    } else if ((prot_str_len == 2) && (strncmp(prot_str, "no", 2) == 0)) {
        s->protected_mode = 0;
    } else {
        p->has_error = 1;
        p->error = vstr_format("unknown protection mode option: %s", prot_str);
    }
    vstr_free(&str);
}

static void config_parser_parse_tcp_backlog(lexi_server* s, config_parser* p) {
    vstr backlog_vstr = config_parser_read_number_string(p);
    const char* backlog_str = vstr_data(&backlog_vstr);
    size_t i, backlog_str_len = vstr_len(&backlog_vstr);
    int res = 0;
    for (i = 0; i < backlog_str_len; ++i) {
        res = (res * 10) + (backlog_str[i] - '0');
    }
    vstr_free(&backlog_vstr);
    s->tcp_backlog = res;
}

static void config_parser_parse_loglevel(lexi_server* s, config_parser* p) {
    vstr ll_vstr = config_parser_read_basic_string(p);
    const char* ll_str = vstr_data(&ll_vstr);
    size_t ll_str_len = vstr_len(&ll_vstr);
    size_t i;
    for (i = 0; i < loglevel_lookups_len; ++i) {
        loglevel_lookup l = loglevel_lookups[i];
        if (ll_str_len != l.str_len) {
            continue;
        }
        if (strncmp(ll_str, l.str, ll_str_len) != 0) {
            continue;
        }
        s->loglevel = l.level;
        vstr_free(&ll_vstr);
        return;
    }
    p->has_error = 1;
    p->error = vstr_format("unknown log level option: %s", ll_str);
    vstr_free(&ll_vstr);
}

static void config_parser_parse_logfile(lexi_server* s, config_parser* p) {
    vstr lf = config_parser_read_basic_string(p);
    const char* lf_str = vstr_data(&lf);
    size_t lf_str_len = vstr_len(&lf);
    char* path;
    if ((lf_str_len == 2) && (strncmp(lf_str, "\"\"", 2) == 0)) {
        path = NULL;
    } else {
        path = calloc(lf_str_len + 1, sizeof *path);
        if (path == NULL) {
            p->has_error = 1;
            p->error = vstr_from("oom");
            return;
        }
        memcpy(path, lf_str, lf_str_len);
        vstr_free(&lf);
    }
    s->logfile = path;
}

static void config_parser_parse_num_databases(lexi_server* s, config_parser* p) {
    vstr str = config_parser_read_number_string(p);
    size_t i, len = vstr_len(&str);
    const char* num_str = vstr_data(&str);
    uint64_t num = 0;
    for (i = 0; i < len; ++i) {
        num = (num * 10) + (num_str[i] - '0');
    }
    s->num_databases = num;
    vstr_free(&str);
}

static void config_parser_parse_user(lexi_server* s, config_parser* p) {
    user user = {0};
    vstr username = vstr_new();
    vec* categories;
    vec* commands;
    vec* passwords;
    uint32_t flags = 0;
    while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
        vstr_push_char(&username, p->ch);
        config_parser_read_char(p);
    }
    user.username = username;
    if (p->ch == 0 || p->ch == '\n') {
        vec_push(&s->users, &user);
        return;
    }
    categories = vec_new(sizeof(category));
    if (categories == NULL) {
        vstr_free(&username);
        p->has_error = 1;
        p->error = vstr_from("oom");
        return;
    }
    commands = vec_new(sizeof(cmd_type));
    if (commands == NULL) {
        vec_free(categories, NULL);
        vstr_free(&username);
        p->has_error = 1;
        p->error = vstr_from("oom");
        return;
    }
    passwords = vec_new(sizeof(vstr));
    if (passwords == NULL) {
        vec_free(categories, NULL);
        vec_free(commands, NULL);
        vstr_free(&username);
        p->has_error = 1;
        p->error = vstr_from("oom");
        return;
    }
    while (p->ch == ' ') {
        config_parser_read_char(p);
        switch (p->ch) {
        case '+':
            if (config_parser_peek(p) == '$') {
                category cat;
                config_parser_read_char(p);
                config_parser_read_char(p);
                cat = config_parser_parse_category(p);
                if (cat == C_Illegal) {
                    vstr_free(&username);
                    vec_free(categories, NULL);
                    vec_free(commands, NULL);
                    vec_free(passwords, free_vstr_in_vec);
                    return;
                }
                vec_push(&categories, &cat);
            } else {
                cmd_type cmd_type;
                config_parser_read_char(p);
                cmd_type = config_parser_parse_cmd_type(p);
                if (cmd_type == CT_Illegal) {
                    vstr_free(&username);
                    vec_free(categories, NULL);
                    vec_free(commands, NULL);
                    vec_free(passwords, free_vstr_in_vec);
                    return;
                }
                vec_push(&commands, &cmd_type);
            }
            break;
        case '>': {
            vstr password = vstr_new();
            config_parser_read_char(p);
            while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
                vstr_push_char(&password, p->ch);
                config_parser_read_char(p);
            }
            vec_push(&passwords, &password);
        } break;
        default:{
            vstr opt = vstr_new();
            const char* opt_str;
            size_t opt_str_len;
            while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
                vstr_push_char(&opt, p->ch);
                config_parser_read_char(p);
            }
            opt_str = vstr_data(&opt);
            opt_str_len = vstr_len(&opt);
            if ((opt_str_len == 2) && (strncmp(opt_str, "on", 2) == 0)) {
                flags |= USER_ON;
            } else if ((opt_str_len == 3) && (strncmp(opt_str, "off", 3) == 0)) {
                vstr_free(&opt);
                continue;
            } else if ((opt_str_len == 6) && (strncmp(opt_str, "nopass", 6) == 0)) {
                flags |= USER_NO_PASS;
            } else {
                vstr_free(&username);
                vec_free(categories, NULL);
                vec_free(commands, NULL);
                vec_free(passwords, free_vstr_in_vec);
                p->has_error = 1;
                p->error = vstr_format("unknown byte when parsing user: %c", p->ch);
                return;
            }
            vstr_free(&opt);
            break;
        }
        }
    }
    user.username = username;
    user.passwords = passwords;
    user.categories = categories;
    user.commands = commands;
    user.flags = flags;
    vec_push(&s->users, &user);
}

static category config_parser_parse_category(config_parser* p) {
    size_t i;
    vstr s = vstr_new();
    const char* cat_str;
    size_t cat_str_len;
    while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    cat_str = vstr_data(&s);
    cat_str_len = vstr_len(&s);
    for (i = 0; i < cat_str_len; ++i) {
        category_lookup lookup = category_lookups[i];
        if (lookup.str_len != cat_str_len) {
            continue;
        }
        if (strncmp(lookup.str, cat_str, cat_str_len) != 0) {
            continue;
        }
        vstr_free(&s);
        return lookup.cat;
    }
    p->has_error = 1;
    p->error = vstr_format("unknown category: %s\n", cat_str);
    vstr_free(&s);
    return C_Illegal;
}

static cmd_type config_parser_parse_cmd_type(config_parser* p) {
    vstr s = vstr_new();
    const char* cmd_str;
    size_t cmd_str_len;
    size_t i;
    while (p->ch != ' ' && p->ch != '\n' && p->ch != 0) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    cmd_str = vstr_data(&s);
    cmd_str_len = vstr_len(&s);
    for (i = 0; i < cmd_lookups_len; ++i) {
        cmd_lookup lookup = cmd_lookups[i];
        if (lookup.str_len != cmd_str_len) {
            continue;
        }
        if (strncmp(lookup.str, cmd_str, cmd_str_len) != 0) {
            continue;
        }
        vstr_free(&s);
        return lookup.cmd;
    }
    p->has_error = 1;
    p->error = vstr_format("unknown command: %s", cmd_str);
    vstr_free(&s);
    return CT_Illegal;
}

static vstr config_parser_read_basic_string(config_parser* p) {
    vstr s = vstr_new();
    while ((p->ch != '\n') && (p->ch != 0)) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    return s;
}

static vstr config_parser_read_number_string(config_parser* p) {
    vstr s = vstr_new();
    while (isdigit(p->ch)) {
        vstr_push_char(&s, p->ch);
        config_parser_read_char(p);
    }
    return s;
}

static char config_parser_peek(config_parser* p) {
    if (p->pos >= p->input_len) {
        return 0;
    }
    return p->input[p->pos];
}

static void config_parser_read_char(config_parser* p) {
    if (p->pos >= p->input_len) {
        p->ch = 0;
        return;
    }
    p->ch = p->input[p->pos];
    p->pos++;
}

static void config_parser_skip_whitespace(config_parser* p) {
    while ((p->ch == ' ') || (p->ch == '\t') || (p->ch == '\r') ||
           (p->ch == '\n')) {
        config_parser_read_char(p);
    }
}

static void config_parser_skip_comment(config_parser* p) {
    while ((p->ch != '\n') && (p->ch != 0)) {
        config_parser_read_char(p);
    }
}
