// Include guard
#ifndef ATEM_CONNECTION_H
#define ATEM_CONNECTION_H

extern int sockRelay;

void setupRelay(const char* addr);
void processRelayData();
void reconnectRelaySocket();

// Ends include guard
#endif
