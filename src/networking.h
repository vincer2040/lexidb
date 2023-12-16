#ifndef __NETWORKING_H__

#define __NETWORKING_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

int create_tcp_socket(int non_blocking);
int tcp_bind(int sfd, const char* addr_str, uint16_t port);
int tcp_listen(int sfd, int backlog);
int tcp_accept(int socket, struct sockaddr_in* addr_in, socklen_t* addr_len);

int set_reuse_addr(int sfd);
int make_socket_nonblocking(int sfd);

#endif /* __NETWORKING_H__ */
