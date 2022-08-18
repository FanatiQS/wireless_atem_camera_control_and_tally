// Include guard
#ifndef UDP_H
#define UDP_H

#include "./udp_unix.h"

int createSocket();
struct sockaddr_in createAddr(const in_addr_t addr);

// Ends include guard
#endif
