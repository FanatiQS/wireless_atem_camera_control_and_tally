#include <stdio.h> // printf, perror, fprintf, snprintf, stderr, stdout, FILE, sprintf
#include <string.h> // bzero, strcmp
#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS, atoi, rand, srand
#include <time.h> // time, time_t, localtime, asctime, clock

#include <sys/socket.h> // recv, sendto, SOCK_DGRAM, AF_INET, socket, SOCK_STREAM, connect, sockaddr, send
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, select, timeval
#include <arpa/inet.h> // inet_addr, htons, sockaddr_in
#include <unistd.h> // usleep

#include "../src/atem.h"



#define PAD_LEN_MAX 5
#define FLAG_ATEM_REQUEST_RESEND 0x40
#define FLAG_ATEM_SYN 0x10
#define OPCODE_ATEM_CLOSED 0x05
#define OPCODE_ATEM_INDEX 12
#define SYN_LEN 20
#define ACK_LEN 12
#define CAMERACONTROL_COMMAND_INDEX 1
#define CAMERACONTROL_PARAMETER_INDEX 2
#define CAMERACONTROL_CMD_LEN 32
#define CAMERACONTROL_CMD_LEN2 24
#define PROTOCOLVERSION_CMD_LEN 12
#define ATEM_INDEX_FLAG 0

#ifndef CLAMP_LEN
#define CLAMP_LEN 24
#endif



// Prints current time
void printTime(FILE* dest) {
	const time_t t = time(NULL);
	fprintf(dest, "%.8s - ", asctime(localtime(&t)) + 11);
}

// Prints uint8 buffer in hex
const char hexTable[] = "0123456789abcdef";
void printBuffer(FILE* dest, const uint8_t *buf, const uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		fprintf(dest, "%c%c ", hexTable[buf[i] >> 4], hexTable[buf[i] & 0x0f]);
	}
	fprintf(dest, "\n");
}

// Pad message to start printing buffer at same terminal X position
void printPaddingForInt(size_t number) {
	for (int i = snprintf(NULL, 0, "%zu", number); i < PAD_LEN_MAX; i++) {
		printf(" ");
	}
}

// Clamps a buffer length to fit on one line
uint16_t clampBufferLen(uint16_t len) {
	return (len < CLAMP_LEN) ? len : CLAMP_LEN;
}

// Table for how to print tally states
char* tallyTable[] = { "NONE", "\x1b[31mPGM\x1b[0m", "\x1b[32mPVW\x1b[0m" };



int main(int argc, char** argv) {
	// Prints usage text if not enough arguments are given
	if (argc < 2) {
		printf("Usage: %s hostIp [flags]\n", argv[0]);
		printf("Flags:\n\t\
--cameraId <number>: The camera id to filter out tally and/or camera control data for\n\t\
--autoReconnect: Automatically reconnects if timed out or requested\n\t\
--closeConnectionAt <number>: Sends a close request after n packets has been received\n\t\
--packetDropChanceSend <chance>: The percentage for a sending packet to be dropped\n\t\
--packetDropChanceRecv <chance>: The percentage for a receiving packet to be dropped\n\t\
--packetDropChanceSeed <seed>: The random seed to use, defaults to random number\n\t\
--packetDropStartSend <number>: The number of packets to allow to send before start dropping packets\n\t\
--packetDropStartRecv <number>: The number of packets to allow to receive before start dropping packets\n\t\
--packetTimeoutAt <number>: The packet in order that should be simulated to be timed out\n\t\
--packetResetDropAtTimeout: Resets packet dropping after a connection times out and restarts\n\t\
--printSeparate: Prints a double new line between each cycle of the infinite loop\n\t\
--printSend: Prints sent data\n\t\
--printDroppedSend: Prints dropped send data\n\t\
--printRecv: Prints received data\n\t\
--printDroppedRecv: Prints dropped receive data\n\t\
--printLastRemoteId: Prints the stored value lastRemoteId\n\t\
--printCommands: Prints all commands\n\t\
--printProtocolVersion: Prints the protocol version received from the switcher\n\t\
--printTally: Prints the tally state for cameraId when it is updated\n\t\
--printTallySource: Prints the tally buffer before it gets translated\n\t\
--printTallyTranslated: Prints the translated tally packet when tally is being updated\n\t\
--printCameraControl: Prints camera control data received for cameraId\n\t\
--printCameraControlSource: Prints the camera control buffer before it gets translated\n\t\
--printCameraControlTranslated: Prints the translated camera control data when received for any camera\n\t\
--tallySources: Sets the number of tally sources that model of ATEM is expected to have\n\t\
--customCountdown <number>: Sets the number of packets to wait until running some custom code\n\t\
--help: Prints usage text\n\t\
--tcpRelay = Relays data to TCP server on localhost\n");
		exit(EXIT_FAILURE);
	}

	// Sets host address to value of first command line argument
	char* addr = argv[1];

	// Initializes flags
	uint8_t camid;
	uint32_t packetDropChanceSend = 0;
	uint32_t packetDropStartSend = 0;
	uint32_t packetDropChanceRecv = 0;
	uint32_t packetDropStartRecv = 0;
	uint32_t packetDropChanceSeed = 0;
	int32_t packetTimeoutAt = -1;
	bool packetResetDropAtTimeout = false;
	bool flagAutoReconnect = false;
	int32_t closeConnectionAt = -1;
	bool flagPrintSeparate = false;
	bool flagPrintSend = false;
	bool flagPrintDroppedSend = false;
	bool flagPrintRecv = false;
	bool flagPrintDroppedRecv = false;
	bool flagPrintLastRemoteId = false;
	bool flagPrintCommands = false;
	bool flagPrintProtocolVersion = false;
	bool flagPrintTally = false;
	bool flagPrintTallySource = false;
	bool flagPrintTallyTranslated = false;
	bool flagPrintCameraControl = false;
	bool flagPrintCameraControlSource = false;
	bool flagPrintCameraControlTranslated = false;
	bool flagRelay = false;
	int32_t tallySources = -1;
	int customCountdown = -1;

	// Parses command line arguments
	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--help")) {
			char* charptrptr[] = { argv[0] };
			main(1, charptrptr);
		}
		else if (!strcmp(argv[i], "--cameraId")) {
			camid = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetDropChanceSend")) {
			packetDropChanceSend = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetDropStartSend")) {
			packetDropStartSend = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetDropChanceRecv")) {
			packetDropChanceRecv = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetDropStartRecv")) {
			packetDropStartRecv = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetDropChanceSeed")) {
			packetDropChanceSeed = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetTimeoutAt")) {
			packetTimeoutAt = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--packetResetDropAtTimeout")) {
			packetResetDropAtTimeout = true;
		}
		else if (!strcmp(argv[i], "--autoReconnect")) {
			flagAutoReconnect = true;
		}
		else if (!strcmp(argv[i], "--closeConnectionAt")) {
			closeConnectionAt = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--printSeparate")) {
			flagPrintSeparate = true;
		}
		else if (!strcmp(argv[i], "--printSend")) {
			flagPrintSend = true;
		}
		else if (!strcmp(argv[i], "--printDroppedSend")) {
			flagPrintDroppedSend = true;
		}
		else if (!strcmp(argv[i], "--printRecv")) {
			flagPrintRecv = true;
		}
		else if (!strcmp(argv[i], "--printDroppedRecv")) {
			flagPrintDroppedRecv = true;
		}
		else if (!strcmp(argv[i], "--printLastRemoteId")) {
			flagPrintLastRemoteId = true;
		}
		else if (!strcmp(argv[i], "--printCommands")) {
			flagPrintCommands = true;
		}
		else if (!strcmp(argv[i], "--printProtocolVersion")) {
			flagPrintProtocolVersion = true;
		}
		else if (!strcmp(argv[i], "--printTally")) {
			flagPrintTally = true;
		}
		else if (!strcmp(argv[i], "--printTallySource")) {
			flagPrintTallySource = true;
		}
		else if (!strcmp(argv[i], "--printTallyTranslated")) {
			flagPrintTallyTranslated = true;
		}
		else if (!strcmp(argv[i], "--printCameraControl")) {
			flagPrintCameraControl = true;
		}
		else if (!strcmp(argv[i], "--printCameraControlSource")) {
			flagPrintCameraControlSource = true;
		}
		else if (!strcmp(argv[i], "--printCameraControlTranslated")) {
			flagPrintCameraControlTranslated = true;
		}
		else if (!strcmp(argv[i], "--tallySources")) {
			tallySources = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "--tcpRelay")) {
			flagRelay = true;
		}
		else if (!strcmp(argv[i], "--customCountdown")) {
			customCountdown = atoi(argv[++i]);
		}
		else {
			fprintf(stderr, "Invalid argument [ %s ]\n", argv[i]);
			exit(EXIT_FAILURE);
		}
	}

	// Requires camera id to be defined if tally or camera control is enabled
	if ((flagPrintTally || flagPrintCameraControl) && camid == 0) {
		fprintf(stderr, "Camera ID is required to be defined if --printTally or --printCameraControl is used\n");
		exit(EXIT_FAILURE);
	}

	// Sets random seed for packet dropper to command line argument or random number
	srand((packetDropChanceSeed) ? packetDropChanceSeed : (packetDropChanceSeed = clock()));

	// Prints result from command line arguments
	printf("Host address: %s\n", addr);
	if (camid) printf("Camera index: %d\n", camid);
	if (packetDropChanceSend || packetDropChanceRecv) {
		printf("Packet drop chance seed: %d\n", packetDropChanceSeed);
	}
	printf("\n");

	//!! for tcp relay
	int socktcp;
	if (flagRelay) {
		printf("Relaying data to TCP server\n");
		socktcp = socket(AF_INET, SOCK_STREAM, 0);
		if (socktcp == -1) {
			perror("TCP socket creation failed");
			exit(EXIT_FAILURE);
		}
		struct sockaddr_in servaddrtcp;
		bzero(&servaddrtcp, sizeof(servaddrtcp));
		servaddrtcp.sin_family = AF_INET;
		servaddrtcp.sin_port = htons(ATEM_PORT);
		servaddrtcp.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (connect(socktcp, (struct sockaddr*)&servaddrtcp, sizeof(servaddrtcp)) == 1) {
			perror("tcp connection failed");
			exit(EXIT_FAILURE);
		}
	}

	// Creates the socket
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("Socket creation failed");
		exit(EXIT_FAILURE);
	}

	// Initializes values for select
	struct timeval tv;
	tv.tv_sec = ATEM_TIMEOUT;
	tv.tv_usec = 0;
	fd_set fds;

	// Creates address struct
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(ATEM_PORT);
	servaddr.sin_addr.s_addr = inet_addr(addr);

	// Ensures string address was successfully converted to an int
	if (servaddr.sin_addr.s_addr == -1) {
		printf("Invalid host address: %s\n", addr);
		exit(EXIT_FAILURE);
	}

	// Connects the socket to always send its data to the atem switcher
	if (connect(sock, (const struct sockaddr *) &servaddr, sizeof(servaddr))) {
		printf("Unable to connect to host\n");
		exit(EXIT_FAILURE);
	}

	// Initializes ATEM struct to start handshake
	struct atem_t atem = { camid };
	resetAtemState(&atem);
	int32_t lastRemoteId = 0;

	// Processes received packets until an error occurs
	bool hasClosed = false;
	while (true) {
		// Decrement customCountdown until it reaches 0
		if (customCountdown > -1) customCountdown--;

		// Closes the connection after set number of packets recevied
		if (closeConnectionAt > 0) {
			closeConnectionAt--;
			if (closeConnectionAt == 0) {
				printTime(stdout);
				printf("Sending close request\n");
				closeAtemConnection(&atem);
			}
		}



		// Only send data if last receive was not dropped
		if (atem.writeLen != 0) {
			// Sends data to server with a chance for it to be dropped
			if (packetDropStartSend > 0 || rand() % 100 >= packetDropChanceSend) {
				// Decrement packetDropStartSend until it reaches 0
				if (packetDropStartSend > 0) packetDropStartSend--;

				// Sends data
				ssize_t sentLen = send(sock, atem.writeBuf, atem.writeLen, 0);

				// Ensures all data was written
				if (sentLen != atem.writeLen) {
					printTime(stderr);
					if (sentLen == -1) {
						perror("Got an error sending data");
					}
					else {
						fprintf(stderr, "Got an error sending data, %zd bytes sent and %d expected\n", sentLen, atem.writeLen);
					}
					exit(EXIT_FAILURE);
				}

				// Prints written data if flag is set
				if (flagPrintSend) {
					printTime(stdout);
					printf("Sent %hu bytes: ", atem.writeLen);
					printPaddingForInt(atem.writeLen);
					printBuffer(stdout, atem.writeBuf, atem.writeLen);
				}
			}
			// Prints dropped data if flag is set
			else if (flagPrintDroppedSend) {
				printTime(stdout);
				printf("Dropped a %hu byte send: ", atem.writeLen);
				printPaddingForInt(atem.writeLen);
				printBuffer(stdout, atem.writeBuf, atem.writeLen);
			}
		}



		// Prints new lines between each cycle if flag is set
		if (flagPrintSeparate) printf("\n\n");

		// Await data on socket or times out
		FD_ZERO(&fds);
		if (packetTimeoutAt > 0) packetTimeoutAt--;
		if (packetTimeoutAt != 0) FD_SET(sock, &fds);
		int selectLen = select(sock + 1, &fds, NULL, NULL, &tv);

		// Throws on select error
		if (selectLen == -1) {
			printTime(stderr);
			perror("Select got an error");
			exit(EXIT_FAILURE);
		}

		// Processes timeouts and restarts connection if flag is set
		if (selectLen == 0) {
			printTime(stdout);

			// Restarts connection if flag is set
			if (flagAutoReconnect) {
				if (hasClosed) {
					printf("Restarting connection\n");
					hasClosed = false;
				}
				else {
					printf("Restarting connection due to timeout\n");
				}
				lastRemoteId = 0;
			}
			// Exits client after not receiving any more data after close
			else if (hasClosed) {
				printf("Client exited\n");
				exit(EXIT_SUCCESS);
			}
			// Processes delayed data after forced timeout
			else if (packetTimeoutAt == 0) {
				packetTimeoutAt = -1;
				printf("Processing delayed packets after timeout and reconnects\n");
				lastRemoteId = -1;
			}
			// Exits client after timeout
			else {
				printf("Exiting due to timeout\n");
				exit(EXIT_SUCCESS);
			}

			// Starts a new connection after timeout
			resetAtemState(&atem);

			// Resets packet dropping after timeout if flag is set
			if (packetResetDropAtTimeout) {
				packetDropChanceRecv = 0;
				packetDropChanceSend = 0;
			}
			continue;
		}



		// Reads data from server
		ssize_t recvLen = recv(sock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0);

		// Ensures data was actually read
		if (recvLen <= 0) {
			printTime(stderr);
			if (recvLen == -1) {
				perror("Error receiving data");
			}
			else {
				fprintf(stderr, "Received no data\n");
			}
			exit(EXIT_FAILURE);
		}

		// Random chance for read packet to be dropped
		if (packetDropStartRecv <= 0 && rand() % 100 < packetDropChanceRecv) {
			// Prints dropped read data if flag is set
			if (flagPrintDroppedRecv) {
				printTime(stdout);
				printf("Dropped a %zu byte recv: ", recvLen);
				printPaddingForInt(recvLen);
				printBuffer(stdout, atem.readBuf, clampBufferLen(recvLen));
			}

			// Skips directly to awaiting more data
			atem.writeLen = 0;
			continue;
		}

		// Decrement packetDropStartRecv until it reaches 0
		if (packetDropStartRecv > 0) packetDropStartRecv--;

		// Prints read data if flag is set
		if (flagPrintRecv) {
			printTime(stdout);
			printf("Recv %zu bytes: ", recvLen);
			printPaddingForInt(recvLen);
			printBuffer(stdout, atem.readBuf, clampBufferLen(recvLen));
		}

		// Ensures no more data is received after a connection has been closed
		if (
			hasClosed && !(atem.readBuf[ATEM_INDEX_FLAG] == 0x30 &&
			atem.readBuf[OPCODE_ATEM_INDEX] == ATEM_CONNECTION_CLOSING)
		) {
			printTime(stderr);
			fprintf(stderr, "Got data after it was expected no more data was to arrive\n\t");
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}



		// Makes sure ack id and local id is never set
		if (
			atem.readBuf[4] != 0x00 || atem.readBuf[5] != 0x00 ||
			atem.readBuf[6] != 0x00 || atem.readBuf[7] != 0x00
		) {
			printTime(stderr);
			fprintf(stderr, "The acknowledge identifier or the local identifier was set\n\t");
			printBuffer(stderr, atem.readBuf, recvLen);
			exit(EXIT_FAILURE);
		}

		// Throws on ATEM resend request since nothing has been sent that can be resent
		if (atem.readBuf[ATEM_INDEX_FLAG] & FLAG_ATEM_REQUEST_RESEND) {
			printTime(stderr);
			fprintf(stderr, "Received a resend request\n\t");
			printBuffer(stderr, atem.readBuf, (atem.readBuf[0] & 0x07) << 8 | atem.readBuf[1]);
			exit(EXIT_FAILURE);
		}

		// Processes the received packet
		switch (parseAtemData(&atem)) {
			// Connection is allowed to continue and might have data to read and/or write
			case ATEM_CONNECTION_OK: break;
			// Handles connection rejected
			case ATEM_CONNECTION_REJECTED: {
				printTime(stdout);
				printf("Connection rejected\n");
				hasClosed = true;
				break;
			}
			// Handles server is closing the connection
			case ATEM_CONNECTION_CLOSING: {
				if (lastRemoteId == -1 || hasClosed) break;
				printTime(stdout);
				printf("Connection closed by server\n");
				hasClosed = true;
				break;
			}
			// Handles client closed the connection
			case OPCODE_ATEM_CLOSED: {
				// Got closed response without ever sending a request
				if (closeConnectionAt != 0) {
					printTime(stderr);
					fprintf(stderr, "Connection closed unexpectedly with opcode 0x05\n\t");
					printBuffer(stderr, atem.readBuf, atem.readLen);
					exit(EXIT_FAILURE);
				}

				// Only continues reading to ensure no more data is transmitted
				printTime(stdout);
				printf("Connection successfully closed, initiated by client\n");
				hasClosed = true;
				break;
			}
			// Throws on unknown opcode
			default: {
				printTime(stderr);
				fprintf(stderr, "Unexpected connection status 0x%x\n\t", atem.readBuf[OPCODE_ATEM_INDEX]);
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_FAILURE);
			}
			// Prints and exits for non ACKREQUEST or SYNACK packet flags
			case ATEM_CONNECTION_ERROR: {
				printTime(stderr);
				fprintf(stderr, "Received packet flags without 0x08 or 0x10\n\t");
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_FAILURE);
			}
		}

		// Prints lastRemoteId from the atem_t struct
		if (flagPrintLastRemoteId) printf("Struct value [ lastRemoteId ] = %d\n", atem.lastRemoteId);

		// Ignores checking remote id for delayed packets processed before connection is restarted
		if (lastRemoteId == -1) {
			if (atem.lastRemoteId == 0) lastRemoteId = 0;
		}
		// Validates remote ids to be a resend or next in line
		else {
			if (atem.lastRemoteId != lastRemoteId && atem.lastRemoteId != lastRemoteId + 1) {
				printTime(stderr);
				fprintf(stderr, "Invalid remote id: %d %d\n\t", atem.lastRemoteId, lastRemoteId);
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_FAILURE);
			}
			lastRemoteId = atem.lastRemoteId;
		}

		// Ensures packet length matches protocol defined length
		if (recvLen != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Packet length did not match length from ATEM protocol\n\t");
			fprintf(stderr, "Packet length: %zu\n\tProtocol length: %d\n\tBuffer: ", recvLen, atem.readLen);
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Ensures SYNACK packets are 20 bytes long
		if (atem.readBuf[ATEM_INDEX_FLAG] & FLAG_ATEM_SYN && atem.readLen != SYN_LEN) {
			printTime(stderr);
			fprintf(stderr, "SYNACK packet was %d bytes\n\t", atem.readLen);
			printPaddingForInt(atem.readLen);
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Processes command data in the ATEM packet
		while (hasAtemCommand(&atem)) {
			// Gets command name and length
			const int oldCmdIndex = atem.cmdIndex;
			const uint32_t cmdName = nextAtemCommand(&atem);
			int cmdLen = atem.cmdIndex - oldCmdIndex;

			// Prints command data if flag is set
			if (flagPrintCommands) {
				printTime(stdout);
				printf("%c%c%c%c - %d: ", cmdName >> 24, cmdName >> 16,
					cmdName >> 8, cmdName, cmdLen);
				printPaddingForInt(cmdLen);
				printBuffer(stdout, atem.cmdBuf, clampBufferLen(cmdLen));
			}

			switch (cmdName) {
				case ATEM_CMDNAME_TALLY: {
					// Ensures tally data is structured as expected
					if ((cmdLen / 4) != ((atem.cmdBuf[0] << 8 | atem.cmdBuf[1]) + 13) / 4) {
						printTime(stderr);
						fprintf(stderr, "Tally command did not match expected length:\n\tcmdLen: %d\n\tcmdBuf: ", cmdLen);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Gets number of tally sources
					const int tallyLen = ((atem.cmdBuf[0] << 8) | atem.cmdBuf[1]);

					// Sets number of tally sources if not previously defined
					if (tallySources == -1) {
						tallySources = tallyLen;
					}
					// Ensures number of tally sources matches what was previously defined
					else if (tallyLen != tallySources) {
						printTime(stderr);
						fprintf(stderr, "Number of tally sources did not match\n\ttallySource: %d\n\tBuffer: ", tallySources);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Ensures camera index is within range
					if (atem.dest > tallyLen) {
						printTime(stderr);
						fprintf(stderr, "Camera id out of range for switcher\n\tdest: %d\n\tcmdBuf: ", atem.dest);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Prints tally state for selected camera id if flag is set
					if (tallyHasUpdated(&atem) && flagPrintTally) {
						printTime(stdout);
						const char* label = tallyTable[atem.pgmTally | atem.pvwTally << 1];
						printf("Camera %d (tally) - %s\n", atem.dest, label);
					}

					// Prints tally buffer before translation if flag is set
					if (flagPrintTallySource) {
						printTime(stdout);
						printf("Tally Source Buffer - ");
						printBuffer(stdout, atem.cmdBuf, cmdLen);
					}

					// Translates ATEMs tally protocol to Blackmagics Embedded Tally Protocol
					translateAtemTally(&atem);
					const uint8_t* translatedTallyBuf = atem.cmdBuf;
					const uint16_t translatedTallyLen = atem.cmdLen;

					// Prints translated tally if flag is set
					if (flagPrintTallyTranslated) {
						printTime(stdout);
						printf("Translated Tally Buffer - ");
						printBuffer(stdout, translatedTallyBuf, translatedTallyLen);
					}

					break;
				}
				case ATEM_CMDNAME_CAMERACONTROL: {
					// Ensures camera control data is structured as expected
					if (cmdLen != CAMERACONTROL_CMD_LEN && cmdLen != CAMERACONTROL_CMD_LEN2) {
						printTime(stderr);
						fprintf(stderr, "Camera control data was not %d bytes long\n\t", CAMERACONTROL_CMD_LEN);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Ensures all data types received can be converted correctly
					if (atem.cmdBuf[3] > 0x03 && atem.cmdBuf[3] != 0x80) {
						printTime(stderr);
						fprintf(stderr, "Unknown or unimplemented data type: 0x%x\n\t", atem.cmdBuf[3]);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Ensures assumed zero bytes are never non-zero
					if (
						atem.cmdBuf[4] != 0x00 || atem.cmdBuf[6] != 0x00 ||
						atem.cmdBuf[8] != 0x00 || atem.cmdBuf[10] != 0x00 ||
						atem.cmdBuf[11] != 0x00 || atem.cmdBuf[12] != 0x00
					) {
						printTime(stderr);
						fprintf(stderr, "One of the bytes assumed to be zero was not zero\n\t");
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Only prints camera control data when flag is set
					if (flagPrintCameraControl) {
						// Only prints for selected camera
						if (getAtemCameraControlDest(&atem) != atem.dest) break;

						// Prints timestamp
						printTime(stdout);

						// Destination
						printf("Camera %d (camera control) - ", getAtemCameraControlDest(&atem));

						// Prints category and paramteter
						switch (atem.cmdBuf[CAMERACONTROL_COMMAND_INDEX]) {
							case 0x00: {
								printf("Lens - ");
								switch (atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]) {
									case 0x00: {
										printf("Focus\n");
										break;
									}
									case 0x01: {
										printf("Instantaneous autofocus\n");
										break;
									}
									case 0x02: {
										printf("Aperture (f-stop)\n");
										break;
									}
									case 0x05: {
										printf("Instantaneous auto aperture\n");
										break;
									}
									case 0x09: {
										printf("Set continuous zoom (speed)\n");
										break;
									}
									default: {
										printf("? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
										printBuffer(stdout, atem.cmdBuf, cmdLen);
										break;
									}
								}
								break;
							}
							case 0x01: {
								printf("Video - ");
								switch (atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]) {
									case 0x01: {
										printf("Gain\n");
										break;
									}
									case 0x02: {
										printf("Manual White Balance\n");
										break;
									}
									case 0x05: {
										printf("Exposure (us)\n");
										break;
									}
									case 0x08: {
										printf("Video sharpening level\n");
										break;
									}
									case 0x0d: {
										printf("Gain (db)\n");
										break;
									}
									case 0x10: {
										printf("ND\n");
										break;
									}
									default: {
										printf("? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
										printBuffer(stdout, atem.cmdBuf, cmdLen);
										break;
									}
								}
								break;
							}
							case 0x04: {
								printf("Display - ");
								switch (atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]) {
									case 0x04: {
										printf("Color bars display time (seconds)\n");
										break;
									}
									default: {
										printf("? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
										printBuffer(stdout, atem.cmdBuf, cmdLen);
										break;
									}
								}
								break;
							}
							case 0x08: {
								printf("Color Correction - ");
								switch (atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]) {
									case 0x00: {
										printf("Lift Adjust\n");
										break;
									}
									case 0x01: {
										printf("Gamma Adjust\n");
										break;
									}
									case 0x02: {
										printf("Gain Adjust\n");
										break;
									}
									case 0x03: {
										printf("Offset Adjust\n");
										break;
									}
									case 0x04: {
										printf("Contrast Adjust\n");
										break;
									}
									case 0x05: {
										printf("Luma mix\n");
										break;
									}
									case 0x06: {
										printf("Color Adjust\n");
										break;
									}
									default: {
										printf("? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
										printBuffer(stdout, atem.cmdBuf, cmdLen);
										break;
									}
								}
								break;
							}
							case 0x0b: {
								printf("PTZ Control - ");
								switch (atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]) {
									case 0x00: {
										printf("Pan/Tilt Velocity\n");
										break;
									}
									default: {
										printf("? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
										printBuffer(stdout, atem.cmdBuf, cmdLen);
										break;
									}
								}
								break;
							}
							default: {
								printf("? [%d] ? - ? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_COMMAND_INDEX], atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
								printBuffer(stdout, atem.cmdBuf, cmdLen);
								break;
							}
						}
					}

					// Prints camera control buffer before translation if flag is set
					if (flagPrintCameraControlSource) {
						printTime(stdout);
						printf("Camera Control Source Buffer - ");
						printBuffer(stdout, atem.cmdBuf, cmdLen);
					}

					// Translates ATEMs camera control protocol to the Blackmagic SDI protocol
					translateAtemCameraControl(&atem);
					const uint8_t* translatedCameraControlBuf = atem.cmdBuf;
					const uint16_t translatedCameraControlLen = atem.cmdLen;

					// Prints translated camera control if flag is set
					if (flagPrintCameraControlTranslated) {
						printTime(stdout);
						printf("Translated Camera Control Buffer - ");
						printBuffer(stdout, translatedCameraControlBuf, translatedCameraControlLen);
					}

					//!! tcp relays camera control data to websocket clients
					if (flagRelay) {
						send(socktcp, atem.cmdBuf, atem.cmdLen, 0);
						usleep(100);
					}

					break;
				}
				case ATEM_CMDNAME_VERSION: {
					//!! there should be some kind of version validation available. Maybe a macro defined tested version?

					// Ensures protocol version data is structured as expected
					if (cmdLen != PROTOCOLVERSION_CMD_LEN) {
						printTime(stderr);
						fprintf(stderr, "Protocol version data was not %d bytes long\n\t", PROTOCOLVERSION_CMD_LEN);
						printBuffer(stderr, atem.cmdBuf, cmdLen);
						exit(EXIT_FAILURE);
					}

					// Only print protocol version when flag is set
					if (flagPrintProtocolVersion) {
						printTime(stdout);
						printf("Protocol version: %d.%d\n", protocolVersionMajor(&atem), protocolVersionMinor(&atem));
					}

					break;
				}
				case ('W' << 24 | 'a' << 16 | 'r' << 8 | 'n'): {
					printTime(stderr);
					fprintf(stderr, "Got a warning message from the switcher\n\t");
					fprintf(stderr, "%s\n\t", atem.cmdBuf);
					printBuffer(stderr, atem.cmdBuf, cmdLen);
				}
			}
		}

		// Ensures that parsing of commands was done exactly to the end
		if (atem.cmdIndex != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Structs cmdIndex and readLen were not equal after command data was processed: %d %d\n\t", atem.cmdIndex, atem.readLen);
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}
	}
}
