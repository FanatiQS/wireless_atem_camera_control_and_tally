#include <stdint.h> // uint8_t, uint16_t
#include <stdio.h> // fprintf, stderr, perror
#include <stdlib.h> // exit, EXIT_FAILURE
#include <string.h> // memcpy

#include "./udp.h"
#include "../src/atem.h"
#include "./server.h"
#include "./atem_extra.h"



// Socket for communicating with ATEM switcher
int atemSock;

// State for ATEM connection
struct atem_t atem;
struct timeval atemLastContact;

// Sends the buffered ATEM data to the ATEM switcher
void sendAtem() {
	if (send(atemSock, atem.writeBuf, atem.writeLen, 0) == -1) {
		perror("Failed to send data to ATEM");
	}
}

// Connects to an ATEM switcher
void setupAtemSocket(const char* addr) {
	// Gets UDP socket for ATEM connection
	atemSock = createSocket();

	// Parses string address into an address type
	const in_addr_t atemAddr = inet_addr(addr);
	if (atemAddr == -1) {
		fprintf(stderr, "Invalid address\n");
		exit(EXIT_FAILURE);
	}

	// Connects ATEM socket to automatically send data to correct address
	const struct sockaddr_in atemSockAddr = createAddr(atemAddr);
	if (connect(atemSock, (const struct sockaddr *)&atemSockAddr, sizeof(atemSockAddr))) {
		perror("Failed to connect socket");
		exit(EXIT_FAILURE);
	}

	// Initializes ATEM connection to switcher
	getTime(&atemLastContact);
	atem_connection_reset(&atem);
	sendAtem();
}

// Maintains ATEM connection
void maintainAtemConnection(const bool socketHasData) {
	// Processes ATEM data if available
	if (socketHasData) {
		// Reads data from the ATEM switcher
		if (recv(atemSock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0) == -1) {
			perror("Unable to read data from ATEM");
			return;
		}

		// Parses read buffer from ATEM
		atem_parse(&atem);

		// Sends response to atem switcher if needed
		if (atem.writeLen) sendAtem();

		// Updates time stamp for most recent atem packet
		getTime(&atemLastContact);

		// Loops through all commands to filter out the once we need
		uint8_t* commands = atem.readBuf + ATEM_LEN_HEADER;
		uint16_t commandsLen = 0;
		uint16_t lastCommandIndex = ATEM_LEN_HEADER;
		while (atem_cmd_available(&atem)) {
			// Only continue with the commands we care about
			switch (atem_cmd_next(&atem)) {
				default: break;
				case ATEM_CMDNAME_VERSION: {
					printf("connected atem\n");
					// @todo buffer data to send to future proxy connections
					break;
				}
				case ATEM_CMDNAME_CAMERACONTROL: {
					// @todo buffer data to send to future proxy connections
					break;
				}
				case ATEM_CMDNAME_TALLY: {
					// @todo buffer data to send to future proxy connections
					break;
				}
				//!! currently all commands seem to be needed for software control to connect
			}

			// Includes command in list for broadcast to proxy clients
			uint16_t cmdLen = atem.cmdIndex - lastCommandIndex;
			lastCommandIndex = atem.cmdIndex;
			memcpy(commands + commandsLen, atem.cmdBuf - ATEM_LEN_CMDHEADER, cmdLen + ATEM_LEN_CMDHEADER);
			commandsLen += cmdLen;
		}

		// Broadcasts commands to all proxy clients
		if (commandsLen) {
			broadcastAtemCommands(commands, commandsLen);
		}
	}
	// Tries to reconnect if not getting response from ATEM
	else if (getTimeDiff(atemLastContact) > ATEM_TIMEOUT * 1000) {
		atem_connection_reset(&atem);
		getTime(&atemLastContact);
		sendAtem();
	}
}

//!! only for testing
void restartAtemConnection() {
	atem_connection_close(&atem);
	sendAtem();
	printf("Restarted ATEM connection\n");
}
