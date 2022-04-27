// Include guard
#ifndef UDP_UNIX_H
#define UDP_UNIX_H

#include <sys/socket.h> // connect, bind, send, sendto, recv, recvfrom, socklen_t, sockaddr
#include <netinet/in.h> // in_addr_t, sockaddr_in, INADDR_ANY
#include <arpa/inet.h> // inet_addr
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, FD_ISSET, select
#include <sys/time.h> // timeval, gettimeofday

// Ends include guard
#endif
