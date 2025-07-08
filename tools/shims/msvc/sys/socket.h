// Include guard
#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h> // socklen_t

typedef int ssize_t;

#define SHUT_WR SD_SEND
#define setsockopt(sock, lvl, name, value, len) setsockopt(sock, lvl, name, (const void*)value, len)

void WSAperror(const char* msg);
#define perror(msg) WSAperror(msg)

#endif // SYS_SOCKET_H
