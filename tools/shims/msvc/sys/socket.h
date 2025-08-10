// Include guard
#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h> // socklen_t

#include "../wsa_shim_perror.h" // wsa_shim_perror

#ifdef _MSC_VER
typedef int ssize_t;
#endif // _MSC_VER

#define SHUT_WR SD_SEND
#define setsockopt(sock, lvl, name, value, len) setsockopt(sock, lvl, name, (const void*)value, len)

#define perror(msg) wsa_shim_perror(msg)

#endif // SYS_SOCKET_H
