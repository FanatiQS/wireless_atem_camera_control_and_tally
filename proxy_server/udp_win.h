// Include guard
#ifndef UDP_WIN_H
#define UDP_WIN_H

#include <winsock2.h> // AF_INET, SOCK_DGRAM, socket, connect, bind, send, sendto, recv, recvfrom, sockaddr, sockaddr_in, INADDR_ANY, inet_addr, htons, fd_set, FD_ZERO, FD_SET, FD_ISSET, select, timeval, SOCKET, INVALID_SOCKET, inet_ntoa, timeval
#pragma comment(lib, "ws2_32.lib")

// Defines types not available in windows
typedef int ssize_t;
typedef unsigned long in_addr_t;
typedef int socklen_t;

// Ends include guard
#endif
