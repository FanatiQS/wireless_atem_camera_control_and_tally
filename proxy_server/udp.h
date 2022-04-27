// Include guard
#ifndef UDP_H
#define UDP_H

#include "./udp_unix.h"

int createSocket();
struct sockaddr_in createAddr(const in_addr_t addr);
uint16_t getTimeDiff(struct timeval timestamp);
void getTime(struct timeval* timestamp);

// Ends include guard
#endif
