#ifndef __SOCK_H__

#define __SOCK_H__

#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

int create_tcp_socket(int nonblocking);
uint32_t parse_addr(char* addr_str, size_t len);
int bind_tcp_sock(int socket, uint32_t addr, uint16_t port);
int tcp_listen(int socket, int backlog);
int tcp_accept(int socket, struct sockaddr_in* addr, socklen_t* addr_len);
int set_reuse_addr(int fd);
int make_socket_nonblocking(int fd);
#endif
