#ifndef __CLI_UTIL_H__

#define __CLI_UTIL_H__

#include <stdint.h>

#define LOG(...)                                                               \
    { printf(__VA_ARGS__); }

#define ERROR "ERROR: "

typedef int Client;

char* get_line(const char* prompt);
Client client_create(char* addr, uint16_t port);


#endif
