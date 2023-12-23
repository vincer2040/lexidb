#ifndef __CLAP_H__

#define __CLAP_H__

#include "ht.h"
#include "object.h"
#include "vec.h"
#include "vstr.h"

typedef vec* args;

typedef struct {
    int help_requested;
    int version_requested;
    int has_error;
    vstr error;
    ht args;
} cla;

args args_new(void);
int args_add(args* args, const char* name, const char* short_arg,
             const char* long_arg, const char* description, objectt type);
void args_free(args args);

cla parse_cmd_line_args(args args, int argc, char* argv[]);
object* args_get(cla* cla, const char* name);
int clap_has_error(cla* cla);
const char* clap_error(cla* cla);
int clap_help_requested(cla* cla);
int clap_version_requested(cla* cla);
void clap_free(cla* cla);
void cmd_line_args_free(cla* cla);

#endif /* __CLAP_H__ */
