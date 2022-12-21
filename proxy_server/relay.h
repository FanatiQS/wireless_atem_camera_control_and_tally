// Include guard
#ifndef ATEM_CONNECTION_H
#define ATEM_CONNECTION_H

#include <stdbool.h> // bool

#include "./udp.h" // in_addr_t

extern int sockRelay;

bool setupRelay(void);
bool relayEnable(const in_addr_t atemAddr);
void relayDisable(void);
void processRelayData(void);
void reconnectRelaySocket(void);

// Ends include guard
#endif
