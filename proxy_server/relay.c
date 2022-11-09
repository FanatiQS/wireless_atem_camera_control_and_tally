#include <stdint.h> // uint8_t, uint16_t
#include <stdio.h> // fprintf, stderr, perror
#include <stdlib.h> // abort
#include <string.h> // memcpy

#include "./relay.h"
#include "./udp.h"
#include "../src/atem.h"
#include "./atem_extra.h"
#include "./server.h"
#include "./timer.h"
#include "./cache.h"
#include "./debug.h"



// Socket for communicating with ATEM switcher
int sockRelay;

// State for ATEM connection
static struct atem_t atem;

// Sends the buffered ATEM data to the ATEM switcher
static void sendAtem() {
	if (send(sockRelay, atem.writeBuf, atem.writeLen, 0) == -1) {
		perror("Failed to send data to ATEM");
	}

	DEBUG_PRINT_BUFFER(atem.writeBuf, atem.writeLen, "sent data to relay server");
}

// Sets up socket for ATEM relay connection
void setupRelay() {
	sockRelay = createSocket();
}

// Connects to an ATEM switcher
void relayEnable(const in_addr_t atemAddr) {
	// Connects ATEM socket to automatically send data to correct address
	const struct sockaddr_in sockAddr = createAddr(atemAddr);
	if (connect(sockRelay, (const struct sockaddr *)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect relay socket");
		abort();
	}

	// Initializes ATEM connection to switcher
	dropTimerEnable();
	atem_connection_reset(&atem);
	sendAtem();
}

// Disconnects from an ATEM switcher
void relayDisable() {
	dropTimerDisable();
	atem_connection_close(&atem);
	sendAtem();
}

// Relays and caches ATEM commands
void processRelayData() {
	// Reads data from the ATEM switcher
	ssize_t recvLen = recv(sockRelay, atem.readBuf, ATEM_MAX_PACKET_LEN, 0);
	if (recvLen == -1) {
		perror("Failed to read data from relay server");
		return;
	}

	// Prints received buffer when debug printing is enabled
	DEBUG_PRINT_BUFFER(atem.readBuf, recvLen, "received data from relay server");

	// Parses ATEM packet and lets connection time out if not connected
	switch (atem_parse(&atem)) {
		case ATEM_CONNECTION_OK:
		case ATEM_CONNECTION_CLOSED: break;
		default: return;
	}

	// Sends response to parsed packet
	sendAtem();

	// Restarts timer for detecting ATEM connection drop
	dropTimerRestart();

	// Hands over commands to cache
	if (atem_cmd_available(&atem)) {
		cacheRelayCommands(atem.readBuf + ATEM_LEN_HEADER, atem.readLen - ATEM_LEN_HEADER);
	}
}

// Reconnects to ATEM after connection has dropped
void reconnectRelaySocket() {
	DEBUG_PRINTF("reconnecting to relay server\n");

	atem_connection_reset(&atem);
	sendAtem();
	dropTimerRestart();
}

// only for temporary hack
void restartRelayConnection() {
	DEBUG_PRINTF("restarting connection to relay server\n");
	atem_connection_close(&atem);
	sendAtem();
	dropTimerRestart();
}
