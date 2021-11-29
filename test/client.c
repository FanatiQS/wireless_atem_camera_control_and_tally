#include <stdio.h> // printf, perror, fprintf, snprintf, stderr, stdout, FILE
#include <string.h> // bzero
#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS, atoi, rand, srand
#include <time.h> // time, time_t, localtime,

#include <sys/socket.h> // recv, sendto, sockaddr_in, SOCK_DGRAM, AF_INET, socket
#include <arpa/inet.h> // inet_addr, htons
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, select, timeval
#include <unistd.h> // usleep

#include "../src/atem.h"



#define PAD_LEN_MAX 5
#define FLAG_ATEM_REQUEST_RESEND 0x40
#define FLAG_ATEM_SYN 0x10
#define OPCODE_ATEM_REJECT 0x03
#define OPCODE_ATEM_CLOSING 0x04
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
void padPrint(size_t number) {
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
--packetDropChanceSend: <chance> The percentage for a sending packet to be dropped\n\t\
--packetDropChanceRecv: <chance> The percentage for a receiving packet to be dropped\n\t\
--packetDropChanceSeed: <seed> The random seed to use, defaults to random number\n\t\
--packetDropStartSend: <number> The number of packets to allow to send before start dropping packets\n\t\
--packetTimeoutAt: <number> The packet in order that should be simulated to be timed out\n\t\
--packetDropStartRecv: <number> The number of packets to allow to receive before start dropping packets\n\t\
--printSeparate: Prints a double new line between each cycle of the infinite loop\n\t\
--printSend: Prints sent data\n\t\
--printDroppedSend: Prints dropped send data\n\t\
--printRecv: Prints received data\n\t\
--printDroppedRecv: Prints dropped receive data\n\t\
--printLastRemoteId: Prints the stored value lastRemoteId\n\t\
--printCommands: Prints all commands\n\t\
--printProtocolVersion: Prints the protocol version received from the switcher\n\t\
--printTally: Prints the tally state for cameraId when it is updated\n\t\
--printCameraControl: Prints camera control data received for cameraId\n\t\
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
	bool flagAutoReconnect = 0;
	bool flagPrintSeparate = 0;
	bool flagPrintSend = 0;
	bool flagPrintDroppedSend = 0;
	bool flagPrintRecv = 0;
	bool flagPrintDroppedRecv = 0;
	bool flagPrintLastRemoteId = 0;
	bool flagPrintCommands = 0;
	bool flagPrintProtocolVersion = 0;
	bool flagPrintTally = 0;
	bool flagPrintCameraControl = 0;
	bool flagRelay = 0;

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
		else if (!strcmp(argv[i], "--autoReconnect")) {
			flagAutoReconnect = 1;
		}
		else if (!strcmp(argv[i], "--printSeparate")) {
			flagPrintSeparate = 1;
		}
		else if (!strcmp(argv[i], "--printSend")) {
			flagPrintSend = 1;
		}
		else if (!strcmp(argv[i], "--printDroppedSend")) {
			flagPrintDroppedSend = 1;
		}
		else if (!strcmp(argv[i], "--printRecv")) {
			flagPrintRecv = 1;
		}
		else if (!strcmp(argv[i], "--printDroppedRecv")) {
			flagPrintDroppedRecv = 1;
		}
		else if (!strcmp(argv[i], "--printLastRemoteId")) {
			flagPrintLastRemoteId = 1;
		}
		else if (!strcmp(argv[i], "--printCommands")) {
			flagPrintCommands = 1;
		}
		else if (!strcmp(argv[i], "--printProtocolVersion")) {
			flagPrintProtocolVersion = 1;
		}
		else if (!strcmp(argv[i], "--printTally")) {
			flagPrintTally = 1;
		}
		else if (!strcmp(argv[i], "--printCameraControl")) {
			flagPrintCameraControl = 1;
		}
		else if (!strcmp(argv[i], "--tcpRelay")) {
			flagRelay = 1;
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
		fprintf(stderr, "Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

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

	// Tally state
	uint8_t tally = 0;

	// Initializes ATEM struct to start handshake
	struct atem_t atem;
	resetAtemState(&atem);

	// Processes received packets until an error occurs
	while (1) {
		// Only send data if last receive was not dropped
		if (atem.writeLen != 0) {
			// Sends data to server with a chance for it to be dropped
			if (packetDropStartSend > 0 || rand() % 100 >= packetDropChanceSend) {
				// Decrement packetDropStartSend until it reaches 0
				if (packetDropStartSend > 0) packetDropStartSend--;

				// Sends data
				size_t sentLen = sendto(sock, atem.writeBuf, atem.writeLen, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));

				// Ensures all data was written
				if (sentLen != atem.writeLen) {
					printTime(stderr);
					fprintf(stderr, "Got an error sending data\n");
					exit(EXIT_FAILURE);
				}

				// Prints written data if flag is set
				if (flagPrintSend) {
					printTime(stdout);
					printf("Sent %hu bytes: ", atem.writeLen);
					padPrint(atem.writeLen);
					printBuffer(stdout, atem.writeBuf, atem.writeLen);
				}
			}
			// Prints dropped data if flag is set
			else if (flagPrintDroppedSend) {
				printTime(stdout);
				printf("Dropped a %hu byte send: ", atem.writeLen);
				padPrint(atem.writeLen);
				printBuffer(stdout, atem.writeBuf, atem.writeLen);
			}
		}



		// Prints new lines between each cycle if flag is set
		if (flagPrintSeparate) printf("\n\n");

		// Await data on socket or times out
		fd_set fds;
		FD_ZERO(&fds);
		if (packetTimeoutAt != 0) FD_SET(sock, &fds);
		if (packetTimeoutAt >= 0) packetTimeoutAt--;
		struct timeval tv;
		bzero(&tv, sizeof(tv));
		tv.tv_sec = 1;
		int selectLen = select(sock + 1, &fds, NULL, NULL, &tv);

		// Throws on select error
		if (selectLen == -1) {
			printTime(stderr);
			fprintf(stderr, "Select got an error\n");
			exit(EXIT_FAILURE);
		}

		// Prints message on timeout and restarts connection if flag is set
		if (!selectLen) {
			printTime(stdout);
			printf("Connection timed out\n");
			if (!flagAutoReconnect) {
				printf("Exiting\n");
				exit(EXIT_SUCCESS);
			}
			resetAtemState(&atem);
			printf("Restarting connection\n");
			continue;
		}



		// Reads data from server
		size_t recvLen = recv(sock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0);

		// Ensures data was actually read
		if (recvLen <= 0) {
			printTime(stderr);
			fprintf(stderr, "Received no data: %zu\n", recvLen);
			exit(EXIT_FAILURE);
		}

		// Random chance for read packet to be dropped
		if (packetDropStartRecv <= 0 && rand() % 100 < packetDropChanceRecv) {
			// Prints dropped read data if flag is set
			if (flagPrintDroppedRecv) {
				printTime(stdout);
				printf("Dropped a %zu byte recv: ", recvLen);
				padPrint(recvLen);
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
			padPrint(recvLen);
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
			exit(EXIT_FAILURE);
		}

		// Processes the received packet
		switch (parseAtemData(&atem)) {
			// Returns 1 for non 0x02 SYN packet opcode
			case 1: {
				// Prints message for reject opcode
				if (atem.readBuf[OPCODE_ATEM_INDEX] == OPCODE_ATEM_REJECT) {
					printTime(stdout);
					printf("Connection rejected\n");
				}
				// Prints message for closed opcode
				else if (atem.readBuf[OPCODE_ATEM_INDEX] == OPCODE_ATEM_CLOSED) {
					printTime(stdout);
					printf("Connection successfully closed, initiated by client\n");
				}
				// Throws on unknown opcode
				else {
					printTime(stderr);
					fprintf(stderr, "Unexpected connection status\n");
				}
				exit(EXIT_SUCCESS);
			}
			// Returns -1 for non ACKREQUEST or SYNACK packet flags
			case -1: {
				printTime(stderr);
				fprintf(stderr, "Received packet flags without 0x08 or 0x10\n");
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
			exit(EXIT_FAILURE);
		}

		// Ensures SYNACK packets are 20 bytes long
		if (atem.readBuf[0] & FLAG_ATEM_SYN) {
			if (atem.readLen != SYN_LEN) {
				printTime(stderr);
				fprintf(stderr, "SYNACK packet was %d bytes: ", atem.readLen);
				padPrint(atem.readLen);
				printBuffer(stderr, atem.readBuf, atem.readLen);
				exit(EXIT_FAILURE);
			}
			// Logs extra message if connection is restarted
			else if (atem.readBuf[OPCODE_ATEM_INDEX] == OPCODE_ATEM_CLOSING) {
				printTime(stdout);
				if (!flagAutoReconnect) {
					printf("Connection wanted to restart, exiting\n");
					exit(EXIT_SUCCESS);
				}
				else {
					printf("Connection restarted\n");
				}
			}
		}

		// Processes command data in the ATEM packet
		while (hasAtemCommand(&atem)) {
			const uint32_t cmdName = nextAtemCommand(&atem);

			// Prints command data if flag is set
			if (flagPrintCommands) {
				printTime(stdout);
				printf("%c%c%c%c - %d: ", atem.cmdBuf[-4], atem.cmdBuf[-3], atem.cmdBuf[-2], atem.cmdBuf[-1], atem.cmdLen);
				padPrint(atem.cmdLen);
				printBuffer(stdout, atem.cmdBuf, clampBufferLen(atem.cmdLen));
			}

			switch (cmdName) {
				case ATEM_CMDNAME_TALLY: {
					//!! mockup for arduino
					// if (parseTally(&atem, 4, &tally) > 0) {
					// 	digitalWrite(pgmPin, tally == ATEM_TALLY_PGM);
					// 	digitalWrite(pvwPin, tally == ATEM_TALLY_PVW);
					// }

					// Ensures tally data is structured as expected
					if ((atem.cmdLen / 4) != ((atem.cmdBuf[0] << 8 | atem.cmdBuf[1]) + 10) / 4) {
						printTime(stdout);
						printf("Tally command did not match expected length:\n\tcmdLen: %d\n\tcmdBuf: ", atem.cmdLen);
						printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
					}

					// Prints tally state for selected camera id if flag is set
					switch (parseAtemTally(&atem, camid, &tally)) {
						case -1: {
							printTime(stderr);
							fprintf(stderr, "Camera id out of range for switcher\n");
							exit(EXIT_FAILURE);
						}
						case 0: break;
						default: {
							if (flagPrintTally) {
								printTime(stdout);
								printf("Camera %d (tally) - %s\n", camid, tallyTable[tally]);
							}
							break;
						}
					}

					// Translates ATEMs tally protocol to Blackmagics Embedded Tally Protocol
					if (flagPrintTally) {
						printf("Translated Tally Buffer - ");
						translateAtemTally(&atem);
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
					if (!flagPrintCameraControl) break;

					// Only prints for selected camera
					if (atem.cmdBuf[0] != camid) break;

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
									printf("Focus");
									break;
								}
								case 0x01: {
									printf("Instantaneous autofocus");
									break;
								}
								case 0x02: {
									printf("Aperture (f-stop)");
									break;
								}
								case 0x05: {
									printf("Instantaneous auto aperture");
									break;
								}
								case 0x09: {
									printf("Set continuous zoom (speed)");
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
									printf("Gain");
									break;
								}
								case 0x02: {
									printf("Manual White Balance");
									break;
								}
								case 0x05: {
									printf("Exposure (us)");
									break;
								}
								case 0x08: {
									printf("Video sharpening level");
									break;
								}
								case 0x0d: {
									printf("Gain (db)");
									break;
								}
								case 0x10: {
									printf("ND");
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
									printf("Color bars display time (seconds)");
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
									printf("Lift Adjust");
									break;
								}
								case 0x01: {
									printf("Gamma Adjust");
									break;
								}
								case 0x02: {
									printf("Gain Adjust");
									break;
								}
								case 0x03: {
									printf("Offset Adjust");
									break;
								}
								case 0x04: {
									printf("Contrast Adjust");
									break;
								}
								case 0x05: {
									printf("Luma mix");
									break;
								}
								case 0x06: {
									printf("Color Adjust");
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
									printf("Pan/Tilt Velocity");
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

					printf("\n");

					// Translates ATEMs camera control protocol to the Blackmagic SDI protocol and prints it
					printf("Translated Camera Control Buffer - ");
					translateAtemCameraControl(&atem);
					printBuffer(stdout, atem.cmdBuf, atem.cmdLen);

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
					printf("Protocol version: %d.%d\n", atem.cmdBuf[0] << 8 | atem.cmdBuf[1], atem.cmdBuf[2] << 8 | atem.cmdBuf[3]);
					break;
				}
			}
		}

		// Ensures that parsing of commands was done exactly to the end
		if (atem.cmdIndex != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Structs cmdIndex and readLen were not equal after command data was processed: %d %d\n", atem.cmdIndex, atem.readLen);
			exit(EXIT_FAILURE);
		}
	}
}
