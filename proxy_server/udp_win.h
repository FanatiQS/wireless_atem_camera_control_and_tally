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

#define perror(err) fprintf(stderr, err ": %d\n", WSAGetLastError())

#define abort()\
	do {\
		WSACleanup();\
		abort();\
	} while (0)

// Casts argument to signed integer as expected by standard
#define send(sock, data, ...) send(sock, (const char*)data, __VA_ARGS__)
#define sendto(sock, data, ...) sendto(sock, (const char*)data, __VA_ARGS__)
#define recv(sock, data, ...) recv(sock, (char*)data, __VA_ARGS__)
#define recvfrom(sock, data, ...) recvfrom(sock, (char*)data, __VA_ARGS__)

// Ends include guard
#endif
