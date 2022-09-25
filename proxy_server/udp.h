// Include guard
#ifndef UDP_H
#define UDP_H

#ifdef _WIN32
#include "./udp_win.h"
#else
#include "./udp_unix.h"
#endif

int createSocket();
struct sockaddr_in createAddr(const in_addr_t addr);

// Ends include guard
#endif
