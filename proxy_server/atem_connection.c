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
	send(atemSock, atem.writeBuf, atem.writeLen, 0);
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
	gettimeofday(&atemLastContact, NULL);
	resetAtemState(&atem);
	sendAtem();
}

// Maintains ATEM connection
void maintainAtemConnection(const bool socketHasData) {
	// Processes ATEM data if available
	if (socketHasData) {
		// Reads data from the ATEM switcher
		if (recv(atemSock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0) == -1) {
			closeAtemConnection(&atem);
			sendAtem();
			return;
		}

		// Parses read buffer from ATEM
		parseAtemData(&atem);

		// Sends response to atem switcher
		sendAtem();

		// Updates time stamp for most recent atem packet
		gettimeofday(&atemLastContact, NULL);

		// Loops through all commands to filter out only the once we need
		uint8_t* allCommands = atem.readBuf + ATEM_LEN_HEADER;
		uint8_t* currentCommand = allCommands;
		while (hasAtemCommand(&atem)) {
			// Gets index of command
			uint16_t startIndex = atem.cmdIndex;

			// Filters out interesting commands
			switch (nextAtemCommand(&atem)) {
				default: continue;
				case ATEM_CMDNAME_VERSION: {
					printf("connected atem\n");
					// @todo buffer data to send to future proxy connections
					continue;
				}
				case ATEM_CMDNAME_CAMERACONTROL: {
					// @todo buffer data to send to future proxy connections
					break;
				}
				case ATEM_CMDNAME_TALLY: {
					// @todo buffer data to send to future proxy connections
					break;
				}
			}

			// Filters out interesting command for proxy sockets
			uint16_t cmdLen = atem.cmdIndex - startIndex;
			memcpy(currentCommand, atem.cmdBuf - 8, cmdLen);
			currentCommand += cmdLen;
		}

		// Gets length of packet after filtering out unisteresting commands
		uint16_t writeLen = currentCommand - allCommands;

		// Retransmits the filtered data to proxy connections
		if (writeLen > ATEM_LEN_HEADER) {
			broadcastAtemCommands(allCommands, writeLen);
		}
	}
	// Tries to reconnect if not getting response from ATEM
	else if (getTimeDiff(atemLastContact) > ATEM_TIMEOUT * 1000) {
		resetAtemState(&atem);
		gettimeofday(&atemLastContact, NULL);
		sendAtem();
	}
}

//!! only for testing
void restartAtemConnection() {
	closeAtemConnection(&atem);
	sendAtem();
}
