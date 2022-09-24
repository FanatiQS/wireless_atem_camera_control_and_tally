// Include guard
#ifndef UDP_UNIX_H
#define UDP_UNIX_H

#include <sys/socket.h> // connect, bind, send, sendto, recv, recvfrom, socklen_t, sockaddr, socket, AF_INET, SOCK_DGRAM, ssize_t
#include <netinet/in.h> // in_addr_t, sockaddr_in, INADDR_ANY
#include <arpa/inet.h> // inet_addr, htons, inet_ntoa
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, FD_ISSET, select, timeval

// Ends include guard
#endif
