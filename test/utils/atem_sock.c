#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t, NULL
#include <stdio.h> // printf, fprintf, stderr, perror, stdout
#include <stdlib.h> // abort, getenv
#include <assert.h> // assert

#include <unistd.h> // close
#include <sys/socket.h> // SOCK_DGRAM, ssize_t, connect, struct sockaddr, socklen_t, recvfrom
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // inet_addr, in_addr_t

#include "./simple_socket.h" // simple_socket_create, simple_socket_poll, simple_socket_send, simple_socket_recv, simple_socket_connect_env, simple_socket_listen
#include "../../core/atem_protocol.h" // ATEM_FLAG_SYN, ATEM_FLAG_RETX
#include "../../core/atem.h" // ATEM_PORT, ATEM_PACKET_LEN_MAX
#include "./atem_header.h" // atem_header_len_get, atem_header_flags_get, atem_header_unknownid_get, atem_header_len_get_verify
#include "./logs.h" // logs_print_buffer, logs_find
#include "./timediff.h" // timediff_mark, timediff_get
#include "./atem_sock.h"

#define TIMEOUT_LISTEN 10000



// Verifies ATEM packet length and unknown id
void atem_packet_verify(uint8_t* packet, size_t recvLen) {
	if (logs_find("atem_recv")) {
		printf("Received packet:\n");
		logs_print_buffer(stdout, packet, recvLen);
	}

	// Verifies unknown id
	int checkUnknownId = 0;
	if (checkUnknownId) {
		uint8_t flags = atem_header_flags_get(packet);
		uint16_t unknownId = atem_header_unknownid_get(packet);
		if (flags == ATEM_FLAG_SYN) {
			if (unknownId != 0x003a) {
				fprintf(stderr, "unknown id 0x3a\n");
				abort();
			}
		}
		else if (flags == (ATEM_FLAG_SYN | ATEM_FLAG_RETX)) {
			if (unknownId != 0x00cd) {
				fprintf(stderr, "unknown id 0xcd\n");
				abort();
			}
		}
		else {
			if (unknownId != 0x0000) {
				fprintf(stderr, "unknown id 0x0000\n");
				abort();
			}
		}
	}

	// Verifies packet length
	atem_header_len_get_verify(packet, recvLen);
}



// Creates a UDP socket for communicating with an ATEM switcher or client
int atem_socket_create(void) {
	return simple_socket_create(SOCK_DGRAM);
}

// Closes socket to ATEM switcher or client
void atem_socket_close(int sock) {
	close(sock);
}



// Connects to the ATEM switcher at ATEM_SERVER_ADDR
void atem_socket_connect(int sock) {
	simple_socket_connect_env(sock, ATEM_PORT, "ATEM_SERVER_ADDR");
}

// Listens for an ATEM client to connect
struct sockaddr_in atem_socket_listen(int sock, uint8_t* packet) {
	// Binds socket for receiving ATEM client packets
	simple_socket_listen(sock, ATEM_PORT);

	// Gets ip address to allow client connections from
	const char* envKey = "ATEM_CLIENT_ADDR";
	const char* cmpAddrEnv = getenv(envKey);
	if (cmpAddrEnv == NULL) {
		fprintf(stderr, "Environment variable %s not defined\n", envKey);
		abort();
	}
	const in_addr_t cmpAddr = inet_addr(cmpAddrEnv);

	struct timespec timeoutStart = timediff_mark();
	struct sockaddr_in peerAddr;
	ssize_t recvLen;
	do {
		// Awaits first packet from ATEM client
		if (simple_socket_poll(sock, TIMEOUT_LISTEN - timediff_get(timeoutStart)) != 1) {
			fprintf(stderr, "No client connected\n");
			abort();
		}

		// Receives first packet
		socklen_t peerAddrLen = sizeof(peerAddr);
		recvLen = recvfrom(sock, packet, ATEM_PACKET_LEN_MAX, 0, (struct sockaddr*)&peerAddr, &peerAddrLen);
		assert(peerAddrLen == sizeof(peerAddr));
		if (recvLen <= 0) {
			if (recvLen == -1) {
				perror("Failed to recvfrom packet");
			}
			else if (recvLen == 0) {
				fprintf(stderr, "Received empty packet from client\n");
			}
			else {
				fprintf(stderr, "Failed to recvfrom: %zu\n", recvLen);
			}
			abort();
		}
	} while (peerAddr.sin_addr.s_addr != cmpAddr);

	// Verifies received ATEM packet
	atem_packet_verify(packet, (size_t)recvLen);

	// Connects socket to ATEM client
	if (connect(sock, (struct sockaddr*)&peerAddr, sizeof(peerAddr))) {
		perror("Failed to connect socket");
		abort();
	}

	// Returns connection information for connected client
	return peerAddr;
}



// Sends ATEM packet
void atem_socket_send(int sock, uint8_t* packet) {
	// Gets packet length from packet
	uint16_t len = atem_header_len_get(packet);

	if (logs_find("atem_send")) {
		printf("Sending packet:\n");
		logs_print_buffer(stdout, packet, len);
	}

	// Sends packet to socket
	simple_socket_send(sock, packet, len);
}

// Receives ATEM packet
void atem_socket_recv(int sock, uint8_t* packet) {
	// Receives packet
	size_t recvLen = simple_socket_recv(sock, packet, ATEM_PACKET_LEN_MAX);
	if (recvLen < ATEM_LEN_HEADER) {
		fprintf(stderr, "Received packet from client smaller than ATEM header\n");
		abort();
	}

	// Verifies received ATEM packet
	atem_packet_verify(packet, recvLen);
}

// Receives and prints ATEM packet
void atem_socket_recv_print(int sock) {
	uint8_t buf[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, buf);
	logs_print_buffer(stdout, buf, atem_header_len_get(buf));
}



// Ensures no more data is written when expecting session to be closed
void atem_socket_norecv(int sock) {
	if (simple_socket_poll(sock, ATEM_TIMEOUT_MS - 1) == 0) return;

	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	fprintf(stderr, "Received data when expecting to time out\n");
	abort();
}
