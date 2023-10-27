// Include guard
#ifndef TEST_SOCKET_H
#define TEST_SOCKET_H

#include <stddef.h> // size_t
#include <stdbool.h> // bool

int simple_socket_create(int sockType);
void simple_socket_connect(int sock, int port, const char* envKey);

void simple_socket_write(int sock, void* buf, size_t len);
size_t simple_socket_recv(int sock, void* buf, size_t size);
bool simple_socket_recv_error(int sock, int err, void* buf, size_t* len);

#endif // TEST_SOCKET_H