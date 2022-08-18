#include "./server.h"
#include "./relay.h"



// this is currently just a hacky solution that restarts the relay connection instead of caching anything
void dumpAtemData(struct session_t* session) {
	reconnectRelaySocket();
}

