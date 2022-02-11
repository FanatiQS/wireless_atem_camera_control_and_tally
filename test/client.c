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
#define CAMERACONTROL_DESTINATION_INDEX 0
#define CAMERACONTROL_COMMAND_INDEX 1
#define CAMERACONTROL_PARAMETER_INDEX 2
#define CAMERACONTROL_CMD_LEN 32
#define CAMERACONTROL_CMD_LEN2 24
#define PROTOCOLVERSION_CMD_LEN 12
#define CLAMP_LEN 32



// Prints current time
void printTime(FILE* dest) {
	const time_t t = time(NULL);
	fprintf(dest, "%.8s - ", asctime(localtime(&t)) + 11);
}

// Prints uint8 buffer in hex
const char hexTable[] = "0123456789abcdef";
void printBuffer(FILE* dest, uint8_t *buf, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		fprintf(dest, "%c%c ", hexTable[buf[i] >> 4], hexTable[buf[i] & 0x0f]);
	}
	fprintf(dest, "\n");
}

// Pad message to start printing buffer at same terminal X position
void printPaddedInt(size_t number) {
	int i = snprintf(NULL, 0, "%zu", number);
	while (i < PAD_LEN_MAX) {
		printf(" ");
		i++;
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
	int customCountdown = -1;

	// Parses command line arguments
	for (int i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--help")) {
			char* charptrptr[] = { argv[0] };
			main(1, charptrptr);
			exit(EXIT_SUCCESS);
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
			perror("TCP socket creation failed\n");
			exit(EXIT_FAILURE);
		}
		struct sockaddr_in servaddrtcp;
		bzero(&servaddrtcp, sizeof(servaddrtcp));
		servaddrtcp.sin_family = AF_INET;
		servaddrtcp.sin_port = htons(ATEM_PORT);
		servaddrtcp.sin_addr.s_addr = inet_addr("127.0.0.1");
		if (connect(socktcp, (struct sockaddr*)&servaddrtcp, sizeof(servaddrtcp)) == 1) {
			perror("tcp connection failed\n");
			exit(EXIT_FAILURE);
		}
	}

	// Creates the socket
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	// Initializes values for select
	struct timeval tv;
	tv.tv_sec = ATEM_TIMEOUT / 1000;
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

	// Initializes ATEM struct to start handshake
	struct atem_t atem = { camid };
	resetAtemState(&atem);

	// Processes received packets until an error occurs
	while (true) {
		// Decrement customCountdown until it reaches 0
		if (customCountdown > -1) customCountdown--;

		// Closes the connection after set number of packets recevied
		if (closeConnectionAt > -1) {
			if (closeConnectionAt == 0) closeAtemConnection(&atem);
			closeConnectionAt--;
		}



		// Only send data if last receive was not dropped
		if (atem.writeLen != 0) {
			// Sends data to server with a chance for it to be dropped
			if (packetDropStartSend > 0 || rand() % 100 >= packetDropChanceSend) {
				// Decrement packetDropStartSend until it reaches 0
				if (packetDropStartSend > 0) packetDropStartSend--;

				// Sends data
				ssize_t sentLen = sendto(sock, atem.writeBuf, atem.writeLen, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

				// Ensures all data was written
				if (sentLen != atem.writeLen) {
					printTime(stderr);
					if (sentLen == -1) {
						perror("Got an error sending data\n");
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
					printPaddedInt(atem.writeLen);
					printBuffer(stdout, atem.writeBuf, atem.writeLen);
				}
			}
			// Prints dropped data if flag is set
			else if (flagPrintDroppedSend) {
				printTime(stdout);
				printf("Dropped a %hu byte send: ", atem.writeLen);
				printPaddedInt(atem.writeLen);
				printBuffer(stdout, atem.writeBuf, atem.writeLen);
			}
		}



		// Prints new lines between each cycle if flag is set
		if (flagPrintSeparate) printf("\n\n");

		// Await data on socket or times out
		FD_ZERO(&fds);
		if (packetTimeoutAt != 0) FD_SET(sock, &fds);
		if (packetTimeoutAt >= 0) packetTimeoutAt--;
		int selectLen = select(sock + 1, &fds, NULL, NULL, &tv);

		// Throws on select error
		if (selectLen == -1) {
			printTime(stderr);
			perror("Select got an error\n");
			exit(EXIT_FAILURE);
		}

		// Prints message on timeout and restarts connection if flag is set
		if (selectLen == 0) {
			printTime(stdout);
			printf("Connection timed out\n");
			if (!flagAutoReconnect) {
				printf("Exiting\n");
				exit(EXIT_SUCCESS);
			}
			resetAtemState(&atem);
			printf("Restarting connection\n");

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
				perror("Error receiving data\n");
			}
			else {
				fprintf(stderr, "Received no data: %zu\n", recvLen);
			}
			exit(EXIT_FAILURE);
		}

		// Random chance for read packet to be dropped
		if (packetDropStartRecv <= 0 && rand() % 100 < packetDropChanceRecv) {
			// Prints dropped read data if flag is set
			if (flagPrintDroppedRecv) {
				printTime(stdout);
				printf("Dropped a %zu byte recv: ", recvLen);
				printPaddedInt(recvLen);
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
			printPaddedInt(recvLen);
			printBuffer(stdout, atem.readBuf, clampBufferLen(recvLen));
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
		if (atem.readBuf[0] & FLAG_ATEM_REQUEST_RESEND) {
			printTime(stderr);
			fprintf(stderr, "Received a resend request\n");
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Processes the received packet
		switch (parseAtemData(&atem)) {
			// Connectin is allowed to continue and might have data to read and/or write
			case ATEM_CONNECTION_OK: break;
			// Prints message for reject opcode
			case ATEM_CONNECTION_REJECTED: {
				printTime(stdout);
				printf("Connection rejected\n");
				if (!flagAutoReconnect) exit(EXIT_SUCCESS);
				break;
			}
			// Prints message if server is closing the connection
			case ATEM_CONNECTION_CLOSING: {
				printTime(stdout);
				if (!flagAutoReconnect) {
					printf("Connection is closing, exiting\n");
					exit(EXIT_SUCCESS);
				}
				printf("Connection is closing, restarting\n");
				break;
			}
			// Prints message for closed opcode
			case OPCODE_ATEM_CLOSED: {
				printTime(stdout);
				printf("Connection successfully closed, initiated by client\n");
				exit(EXIT_SUCCESS);
				break;
			}
			// Throws on unknown opcode
			default: {
				printTime(stderr);
				fprintf(stderr, "Unexpected connection status %x\n", atem.readBuf[OPCODE_ATEM_INDEX]);
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_SUCCESS);
			}
			// Prints and exits for non ACKREQUEST or SYNACK packet flags
			case ATEM_CONNECTION_ERROR: {
				printTime(stderr);
				fprintf(stderr, "Received packet flags without 0x08 or 0x10\n");
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_FAILURE);
			}
		}

		// Prints values from the atem_t struct
		if (flagPrintLastRemoteId) printf("Struct value [ lastRemoteId ] = %d\n", atem.lastRemoteId);

		// Ensures packet length matches protocol defined length
		if (recvLen != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Packet length did not match length from ATEM protocol\n\t");
			fprintf(stderr, "Packet length: %zu\n\tProtocol length: %d\n", recvLen, atem.readLen);
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Ensures SYNACK packets are 20 bytes long
		if (atem.readBuf[0] & FLAG_ATEM_SYN && atem.readLen != SYN_LEN) {
			printTime(stderr);
			fprintf(stderr, "SYNACK packet was %d bytes: ", atem.readLen);
			printPaddedInt(atem.readLen);
			printBuffer(stderr, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Processes command data in the ATEM packet
		while (hasAtemCommand(&atem)) {
			const uint32_t cmdName = nextAtemCommand(&atem);

			// Prints command data if flag is set
			if (flagPrintCommands) {
				printTime(stdout);
				printf("%c%c%c%c - %d: ", atem.cmdBuf[-4], atem.cmdBuf[-3], atem.cmdBuf[-2], atem.cmdBuf[-1], atem.cmdLen);
				printPaddedInt(atem.cmdLen);
				printBuffer(stdout, atem.cmdBuf, clampBufferLen(atem.cmdLen));
			}

			switch (cmdName) {
				case ATEM_CMDNAME_TALLY: {
					// Ensures tally data is structured as expected
					if ((atem.cmdLen / 4) != ((atem.cmdBuf[0] << 8 | atem.cmdBuf[1]) + 13) / 4) {
						printTime(stdout);
						printf("Tally command did not match expected length:\n\tcmdLen: %d\n\tcmdBuf: ", atem.cmdLen);
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
						exit(EXIT_FAILURE);
					}

					// Ensures camera index is within range
					if (atem.dest > ((atem.cmdBuf[0] << 8) | atem.cmdBuf[1])) {
						printTime(stderr);
						fprintf(stderr, "Camera id out of range for switcher\n");
						printBuffer(stderr, atem.readBuf, atem.readLen);
						exit(EXIT_FAILURE);
					}

					// Prints tally state for selected camera id if flag is set
					if (tallyHasUpdated(&atem) && flagPrintTally) {
						printTime(stdout);
						const uint8_t tally = atem.pgmTally | atem.pvwTally << 1;
						printf("Camera %d (tally) - %s\n", atem.dest, tallyTable[tally]);
					}

					// Prints tally buffer before translation if flag is set
					if (flagPrintTallySource) {
						printf("Tally Source Buffer - ");
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
					}

					// Translates ATEMs tally protocol to Blackmagics Embedded Tally Protocol
					translateAtemTally(&atem);
					if (flagPrintTallyTranslated) {
						printf("Translated Tally Buffer - ");
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
					}

					break;
				}
				case ATEM_CMDNAME_CAMERACONTROL: {
					// Ensures camera control data is structured as expected
					if (atem.cmdLen != CAMERACONTROL_CMD_LEN && atem.cmdLen != CAMERACONTROL_CMD_LEN2) {
						printTime(stderr);
						fprintf(stderr, "Camera control data was not %d bytes long\n\t", CAMERACONTROL_CMD_LEN);
						printBuffer(stderr, atem.cmdBuf, atem.cmdLen);
						exit(EXIT_FAILURE);
					}

					// Ensures all data type received can be converted correctly
					if (atem.cmdBuf[3] > 0x03 && atem.cmdBuf[3] != 0x80) {
						printTime(stderr);
						fprintf(stderr, "Unknown or unimplemented data type: %x\n", atem.cmdBuf[3]);
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
						printBuffer(stderr, atem.cmdBuf, atem.cmdLen);
						exit(EXIT_FAILURE);
					}

					// Only print camera control data when flag is set
					if (flagPrintCameraControl) {
						// Only prints for selected camera
						if (getAtemCameraControlDest(&atem) != atem.dest) break;

						// Prints timestamp
						printTime(stdout);

						// Destination
						printf("Camera %d (camera control) - ", atem.cmdBuf[CAMERACONTROL_DESTINATION_INDEX]);

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
										printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
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
										printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
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
										printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
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
										printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
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
										printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
										break;
									}
								}
								break;
							}
							default: {
								printf("? [%d] ? - ? [%d] ?\n\t", atem.cmdBuf[CAMERACONTROL_COMMAND_INDEX], atem.cmdBuf[CAMERACONTROL_PARAMETER_INDEX]);
								printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
								break;
							}
						}
					}

					// Prints camera control buffer before translation if flag is set
					if (flagPrintCameraControlSource) {
						printf("Camera Control Source Buffer - ");
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
					}

					// Translates ATEMs camera control protocol to the Blackmagic SDI protocol and prints it
					translateAtemCameraControl(&atem);
					if (flagPrintCameraControlTranslated) {
						printf("Translated Camera Control Buffer - ");
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
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
					if (atem.cmdLen != PROTOCOLVERSION_CMD_LEN) {
						printTime(stderr);
						fprintf(stderr, "Protocol version data was not %d bytes long\n\t", PROTOCOLVERSION_CMD_LEN);
						printBuffer(stderr, atem.cmdBuf, atem.cmdLen);
						exit(EXIT_FAILURE);
					}

					// Only print protocol version when flag is set
					if (!flagPrintProtocolVersion) break;

					// Prints protocol version
					printTime(stdout);
					printf("Protocol version: %d.%d\n", protocolVersionMajor(&atem), protocolVersionMinor(&atem));
					break;
				}
				case ('W' << 24 | 'a' << 16 | 'r' << 8 | 'n'): {
					printTime(stderr);
					fprintf(stderr, "Got a warning message from the switcher\n\t");
					fprintf(stderr, "%s\n\t", atem.cmdBuf);
					printBuffer(stderr, atem.cmdBuf, atem.cmdLen);
				}
			}
		}

		// Ensures that parsing of commands was done exactly to the end
		if (atem.cmdIndex != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Structs cmdIndex and readLen were not equal after command data was processed: %d %d\n", atem.cmdIndex, atem.readLen);
			printBuffer(stdout, atem.readBuf, atem.readLen);
			exit(EXIT_FAILURE);
		}
	}
}
