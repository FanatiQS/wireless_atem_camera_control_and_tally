// Include guard
#ifndef SIMPLE_SOCKET_H
#define SIMPLE_SOCKET_H

#include <stddef.h> // size_t
#include <stdbool.h> // bool

#define SIMPLE_SOCKET_MAX_FD (65535)

int simple_socket_create(int sockType);
void simple_socket_connect(int sock, int port, const char* envKey);

void simple_socket_send(int sock, const void* buf, size_t len);
size_t simple_socket_recv(int sock, void* buf, size_t size);
bool simple_socket_recv_error(int sock, int err, void* buf, size_t* len);
int simple_socket_poll(int sock, int timeout);

#endif // SIMPLE_SOCKET_H
