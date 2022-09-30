// Include guard
#ifndef ATEM_CONNECTION_H
#define ATEM_CONNECTION_H

extern int sockRelay;

void setupRelay();
void relayEnable(const char* addr);
void relayDisable();
void processRelayData();
void reconnectRelaySocket();

// Ends include guard
#endif
