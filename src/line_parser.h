#ifndef __LINE_PARSER_H__

#define __LINE_PARSER_H__

#include "cmd.h"
#include <stddef.h>

cmd parse_line(const char* line, size_t line_len);

#endif /* __LINE_PARSER_H__ */
