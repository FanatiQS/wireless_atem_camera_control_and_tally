#include <stdint.h> // uint8_t, uint16_t

#include "./cache.h"
#include "./server.h"
#include "./relay.h"



// this is currently just a hacky solution that restarts the relay connection instead of caching anything
void restartRelayConnection(void);
void dumpAtemData(struct session_t* session) {
	restartRelayConnection();
}

// Broadcasts relay commands to proxy connections
void cacheRelayCommands(uint8_t* commands, uint16_t len) {
	broadcastAtemCommands(commands, len);
}

