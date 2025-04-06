// Include guard
#ifndef SIMPLE_SOCKET_H
#define SIMPLE_SOCKET_H

#include <stddef.h> // size_t
#include <stdbool.h> // bool
#include <stdint.h> // uint16_t

#include <netinet/in.h> // in_addr_t

#define SIMPLE_SOCKET_MAX_FD (0xffff)

int simple_socket_create(int type);
void simple_socket_connect(int sock, uint16_t port, in_addr_t addr);
void simple_socket_connect_env(int sock, uint16_t port, const char* env_key);
void simple_socket_listen(int sock, uint16_t port);

void simple_socket_send(int sock, const void* buf, size_t len);
size_t simple_socket_recv(int sock, void* buf, size_t size);
bool simple_socket_recv_error(int sock, int err, void* buf, size_t* len);
bool simple_socket_poll(int sock, int timeout);

#endif // SIMPLE_SOCKET_H
