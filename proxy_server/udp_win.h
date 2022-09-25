// Include guard
#ifndef UDP_WIN_H
#define UDP_WIN_H

#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // AF_INET, SOCK_DGRAM, socket, connect, bind, send, sendto, recv, recvfrom, sockaddr, sockaddr_in, INADDR_ANY, inet_addr, htons, fd_set, FD_ZERO, FD_SET, FD_ISSET, select, timeval, SOCKET, INVALID_SOCKET, inet_ntoa, timeval, WSAStartup, WSADATA, MAKEWORD, WSAGetLastError, WSACleanup
#pragma comment(lib, "ws2_32.lib")

// Defines types not available in windows
typedef int ssize_t;
typedef unsigned long in_addr_t;
typedef int socklen_t;

#define perror(...)\
	do {\
		fprintf(stderr, __VA_ARGS__);\
		fprintf(stderr, ": %d\n", WSAGetLastError());\
	} while (0)

#define abort()\
	do {\
		WSACleanup();\
		abort();\
	} while (0)

// Ends include guard
#endif
