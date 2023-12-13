#ifndef  __PARSER_H__

#define __PARSER_H__

#include "cmd.h"
#include <stddef.h>
#include <stdint.h>

cmd parse(const uint8_t* input, size_t input_len);

#endif /* __PARSER_H__ */
