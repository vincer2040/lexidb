#ifndef __VNET_H__

#define __VNET_H__

#include <sys/types.h>
#include <stdint.h>
#include <netinet/in.h>

#define VNET_OK 0
#define VNET_ERR -1

int vnet_tcp_server(const char* addr, uint16_t port, int backlog);
int vnet_tcp6_server(char* addr, uint16_t port, int backlog);
int vnet_accept(int socket, char* ip, size_t ip_len, int* port);

#endif /* __VNET_H__ */
