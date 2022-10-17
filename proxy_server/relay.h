// Include guard
#ifndef ATEM_CONNECTION_H
#define ATEM_CONNECTION_H

extern int sockRelay;

void setupRelay();
void relayEnable(const in_addr_t atemAddr);
void relayDisable();
void processRelayData();
void reconnectRelaySocket();

// Ends include guard
#endif
