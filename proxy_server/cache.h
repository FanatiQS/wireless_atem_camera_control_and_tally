// Include guard
#ifndef CACHE_H
#define CACHE_H

#include <stdint.h> // uint8_t, uint16_t

#include "./server.h" // session_t

void dumpAtemData(struct session_t* session);
void cacheRelayCommands(uint8_t* commands, uint16_t len);

// Ends include guard
#endif
