#include <stdio.h> // printf, perror, fprintf, stderr, stdout, FILE
#include <string.h> // strlen, bzero
#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS, atoi
#include <time.h> // time, struct tm, time_t, localtime,

#include <sys/socket.h> // recv, sendto, sockaddr_in, SOCK_DGRAM, AF_INET, socket
#include <arpa/inet.h> // inet_addr, htons
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, select
#include <unistd.h> // sleep

#include "../src/atem.h"



// Prints current time
void printTime(FILE* dest) {
	const time_t t = time(NULL);
	const struct tm *time = localtime(&t);
	fprintf(dest, "%d:%d:%d - ", time->tm_hour, time->tm_min, time->tm_sec);
}

// Prints uint8 buffer in hex
const char hexTable[] = "0123456789abcdef";
void printBuffer(FILE* dest, uint8_t *buf, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		fprintf(dest, "%c%c ", hexTable[buf[i] >> 4], hexTable[buf[i] & 0x0f]);
	}
	fprintf(dest, "\n");
}

// Table for how to print tally states
char* tallyTable[] = { "NONE", "\x1b[31mPGM\x1b[0m", "\x1b[32mPVW\x1b[0m" };



int main(int argc, char** argv) {
	// Prints usage text if not enough arguments are given
	if (argc < 3) {
		printf("Usage: %s hostIp cameraId [flags]\n", argv[0]);
		printf("Flags:\n\tr = Prints received data\n\ts = Prints sent data\n\tc = Prints ignored commands\nx = Relays data to TCP server on localhost\n");
		exit(EXIT_FAILURE);
	}

	// Sets host address, camera id and print flags from command line arguments
	char* addr = argv[1];
	uint8_t camid = atoi(argv[2]);
	bool flagPrintRecv = 0;
	bool flagPrintSend = 0;
	bool flagPrintCommands = 0;
	bool flagPrintStructLastRemoteId = 0;
	bool flagAutoReconnect = 0;
	bool flagRelay = 0;
	if (argv[3] != NULL) {
		for (int i = 0; i < strlen(argv[3]); i++) {
			char c = argv[3][i];
			if (c == 'r') {
				flagPrintRecv = 1;
			}
			else if (c == 's') {
				flagPrintSend = 1;
			}
			else if (c == 'c') {
				flagPrintCommands = 1;
			}
			else if (c == 'x') {
				flagRelay = 1;
			}
		}
	}

	// Prints result from command line arguments
	printf("Host address: %s\nCamera index: %d\n\n", addr, camid);

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

	// Tally state
	uint8_t tally = 0;

	// Initializes ATEM struct to start handshake
	struct atem_t atem;
	resetAtemState(&atem);

	// Processes received packets until an error occurs
	while (1) {
		// Separate prints for each cycle with double line break
		if (flagPrintRecv || flagPrintSend) printf("\n");

		// Sends data to server
		size_t sentLen = sendto(sock, atem.writeBuf, atem.writeLen, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
		if (sentLen != atem.writeLen) {
			printTime(stderr);
			fprintf(stderr, "Got an error sending data\n");
			exit(EXIT_FAILURE);
		}
		if (flagPrintSend) {
			printf("Sent %zu bytes:  \t", sentLen);
			printBuffer(stdout, atem.writeBuf, atem.writeLen);
		}

		// Await data on socket or times out
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);
		struct timeval tv;
		bzero(&tv, sizeof(tv));
		tv.tv_sec = 1;
		int selectLen = select(sock + 1, &fds, NULL, NULL, &tv);
		if (selectLen == -1) {
			printTime(stderr);
			fprintf(stderr, "Select got an error\n");
			exit(EXIT_FAILURE);
		}
		if (!selectLen) {
			printTime(stdout);
			printf("Connection timed out\n");
			resetAtemState(&atem);
			if (!flagAutoReconnect) {
				printf("Exiting\n");
				exit(EXIT_SUCCESS);
			}
			else {
				printf("Restarting connection\n");
			}
			continue;
		}

		// Reads data from server
		size_t recvLen = recv(sock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0);
		if (recvLen <= 0) {
			printTime(stderr);
			fprintf(stderr, "Received no data: %zu\n", recvLen);
			exit(EXIT_FAILURE);
		}
		if (flagPrintRecv) {
			printf("Recv %zu bytes:  \t", recvLen);
			printBuffer(stdout, atem.readBuf, (recvLen > 32) ? 32 : recvLen);
		}

		// Throws on ATEM resend request since nothing has been sent that can be resent
		if (atem.readBuf[0] & 0x40) {
			printTime(stderr);
			fprintf(stderr, "Received a resend request\n");
			exit(EXIT_FAILURE);
		}

		// Processes the received packet
		if (parseAtemData(&atem)) {
			if (atem.readBuf[12] != 0x03) {
				printTime(stderr);
				fprintf(stderr, "Unexpected connection status\n");
				exit(EXIT_FAILURE);
			}
			printTime(stdout);
			printf("Connection rejected\n");
			exit(EXIT_SUCCESS);
		}

		// Prints values from the atem_t struct
		if (flagPrintStructLastRemoteId) printf("Struct value [ lastRemoteId ] = %d\n", atem.lastRemoteId);

		// Makes sure packet length matches protocol defined length
		if (recvLen != atem.readLen) {
			printTime(stderr);
			fprintf(stderr, "Packet length did not match length from ATEM protocol\n\tPacket length: %zu\n\tProtocol length: %d\n", recvLen, atem.readLen);
			exit(EXIT_FAILURE);
		}

		// Logs extra message if connection is restarted
		if (atem.writeLen == 20) {
			printTime(stdout);
			if (!flagAutoReconnect) {
				printf("Connection wanted to restart, exiting\n");
				exit(EXIT_SUCCESS);
			}
			else {
				printf("Connection restarted\n");
			}
		}

		// Processes command data in the ATEM packet
		while (hasAtemCommand(&atem)) {
			switch (parseAtemCommand(&atem)) {
				case ATEM_CMDNAME_TALLY: {
					//!! mockup for arduino
					// if (parseTally(&atem, 4, &tally) > 0) {
					// 	digitalWrite(pgmPin, tally == ATEM_TALLY_PGM);
					// 	digitalWrite(pvwPin, tally == ATEM_TALLY_PVW);
					// }

					// Prints tally state for selected camera id
					if (parseAtemTally(&atem, camid, &tally) > 0) {
						printf("Camera %d (tally) - %s\n", camid, tallyTable[tally]);
					}
					break;
				}
				case ATEM_CMDNAME_CAMERACONTROL: {
					//!! tcp relays camera control data to websocket clients
					if (flagRelay) {
						//!! needs to reorder command buffer to conform to sdi protocol
						send(socktcp, atem.cmdBuf, atem.cmdLen, 0);
						usleep(100);
					}

					// Only prints for selected camera
					if (atem.cmdBuf[0] != camid) break;

					// Destination
					printf("Camera %d (camera control) - ", atem.cmdBuf[0]);

					// Category and paramteter
					switch (atem.cmdBuf[1]) {
						case 0x00: {
							printf("Lens - ");
							switch (atem.cmdBuf[2]) {
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
								case 0x09: {
									printf("Set continuous zoom (speed)");
									break;
								}
								default: {
									printf("? [%d] ?\n\t", atem.cmdBuf[2]);
									printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
									break;
								}
							}
							break;
						}
						case 0x01: {
							printf("Video - ");
							switch (atem.cmdBuf[2]) {
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
									printf("? [%d] ?\n\t", atem.cmdBuf[2]);
									printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
									break;
								}
							}
							break;
						}
						case 0x04: {
							printf("Display - ");
							switch (atem.cmdBuf[2]) {
								case 0x04: {
									printf("Color bars display time (seconds)");
									break;
								}
								default: {
									printf("? [%d] ?\n\t", atem.cmdBuf[2]);
									printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
									break;
								}
							}
							break;
						}
						case 0x08: {
							printf("Color Correction - ");
							switch (atem.cmdBuf[2]) {
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
									printf("? [%d] ?\n\t", atem.cmdBuf[2]);
									printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
									break;
								}
							}
							break;
						}
						case 0x0b: {
							printf("PTZ Control - ");
							switch (atem.cmdBuf[2]) {
								case 0x00: {
									printf("Pan/Tilt Velocity");
									break;
								}
								default: {
									printf("? [%d] ?\n\t", atem.cmdBuf[2]);
									printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
									break;
								}
							}
							break;
						}
						default: {
							printf("? [%d] ? - ? [%d] ?\n\t", atem.cmdBuf[1], atem.cmdBuf[2]);
							printBuffer(stdout, atem.cmdBuf, atem.cmdLen);
							break;
						}
					}

					printf("\n");
					break;
				}
				case ATEM_CMDNAME_VERSION: {
					//!! there should be some kind of version validation available. Maybe a macro defined tested version?
					printf("Protocol version: %d.%d\n", atem.cmdBuf[0] << 8 | atem.cmdBuf[1], atem.cmdBuf[2] << 8 | atem.cmdBuf[3]);
					break;
				}
				default: {
					if (!flagPrintCommands) break;
					printf("%c%c%c%c - %d - ", atem.cmdBuf[-4], atem.cmdBuf[-3], atem.cmdBuf[-2], atem.cmdBuf[-1], atem.cmdLen);
					printBuffer(stdout, atem.cmdBuf, ((atem.cmdLen - 8) < 16) ? atem.cmdLen - 8 : 16);
					break;
				}
			}
		}
	}
}
