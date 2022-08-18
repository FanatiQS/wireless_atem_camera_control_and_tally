#include <stdint.h> // uint8_t, uint16_t
#include <stdio.h> // fprintf, stderr, perror
#include <stdlib.h> // abort
#include <string.h> // memcpy

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

#ifdef DEBUG
	DEBUG_PRINT("sent data to relay server");
	for (uint16_t i = 0; i < atem.writeLen; i++) {
		if ((i % DEBUG_PRINT_LINE_LIMIT) == 0) {
			printf("\n\t");
		}
		printf("%.2x ", atem.writeBuf[i]);
	}
	printf("\n");
#endif
}

// Connects to an ATEM switcher
void setupRelay(const char* addr) {
	// Gets UDP socket for ATEM connection
	sockRelay = createSocket();

	// Parses string address into an address type
	const in_addr_t atemAddr = inet_addr(addr);
	if (atemAddr == -1) {
		fprintf(stderr, "Invalid relay server address\n");
		abort();
	}

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

// Relays and caches ATEM commands
void processRelayData() {
	// Reads data from the ATEM switcher
	ssize_t recvLen = recv(sockRelay, atem.readBuf, ATEM_MAX_PACKET_LEN, 0);
	if (recvLen == -1) {
		perror("Failed to read data from relay server");
		return;
	}

	// Prints received buffer when debug printing is enabled
#ifdef DEBUG
	DEBUG_PRINT("received data from relay server");
	for (uint16_t i = 0; i < recvLen; i++) {
		if ((i % DEBUG_PRINT_LINE_LIMIT) == 0) {
			printf("\n\t");
		}
		printf("%.2x ", atem.readBuf[i]);
	}
	printf("\n");
#endif

	// Parses ATEM packet and let connection time out if not connected
	switch (atem_parse(&atem)) {
		case ATEM_CONNECTION_OK:
		case ATEM_CONNECTION_CLOSED: break;
		default: return;
	}

	// Sends response to parsed packet
	sendAtem();
	dropTimerRestart();

	// caching and filtering not implemented

	// Relays the ATEM payload to proxy connections
	if (atem_cmd_available(&atem)) {
		broadcastAtemCommands(atem.readBuf + ATEM_LEN_HEADER, atem.readLen - ATEM_LEN_HEADER);
	}
}

// Reconnects to ATEM after connection has dropped
void reconnectRelaySocket() {
	DEBUG_PRINT("reconnecting to relay server\n");

	atem_connection_close(&atem);
	sendAtem();
	dropTimerRestart();
}

