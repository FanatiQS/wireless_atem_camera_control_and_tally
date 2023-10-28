#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t
#include <stdio.h> // printf, fprintf, stderr, perror, stdout
#include <stdlib.h> // abort

#include <unistd.h> // close
#include <sys/socket.h> // AF_INET, SOCK_DGRAM, ssize_t, send, recv, bind, connect, struct sockaddr, socklen_t, recvfrom
#include <arpa/inet.h> // htons
#include <netinet/in.h> // struct sockaddr_in

#include "./simple_socket.h" // simple_socket_create, simple_socket_select
#include "../../src/atem_protocol.h" // ATEM_FLAG_SYN, ATEM_FLAG_RETX
#include "../../src/atem.h" // ATEM_PORT, ATEM_MAX_PACKET_LEN
#include "./atem_header.h" // atem_header_len_get, atem_header_flags_get, atem_header_unknownid_get, atem_header_len_get_verify, 
#include "./logs.h" // logs_print_buffer, logs_enable_recv, logs_enable_send
#include "./atem_sock.h"

#define TIMEOUT_LISTEN 10
#define TIMEOUT_WAIT 2



// Verifies ATEM packet length and unknown id
void atem_packet_verify(uint8_t* packet, size_t recvLen) {
	if (logs_enable_recv) {
		printf("Received packet:\n");
		logs_print_buffer(stdout, packet, recvLen);
	}

	// Verifies unknown id
	int checkUnknownId = 0;
	if (checkUnknownId) {
		uint8_t flags = atem_header_flags_get(packet);
		uint8_t unknownId = atem_header_unknownid_get(packet);
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



// Connects to the ATEM switcher at atemServerAddr
void atem_socket_connect(int sock) {
	simple_socket_connect(sock, ATEM_PORT, "ATEM_CLIENT_ADDR");
}

// Listens for an ATEM client to connect
uint16_t atem_socket_listen(int sock, uint8_t* packet) {
	// Binds socket for receiving ATEM client packets
	struct sockaddr_in bindAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = INADDR_ANY
	};
	if (bind(sock, (const struct sockaddr*)&bindAddr, sizeof(bindAddr))) {
		perror("Failed to bind server socket");
		abort();
	}

	// Awaits first packet from ATEM client
	if (simple_socket_select(sock, TIMEOUT_LISTEN) != 1) {
		fprintf(stderr, "No client connected\n");
		abort();
	}

	// Receives first packet
	struct sockaddr_in peerAddr;
	socklen_t peerAddrLen = sizeof(peerAddr);
	ssize_t recvLen = recvfrom(sock, packet, ATEM_MAX_PACKET_LEN, 0, (struct sockaddr*)&peerAddr, &peerAddrLen);
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

	// Verifies received ATEM packet
	atem_packet_verify(packet, recvLen);

	// Connects socket to ATEM client
	if (connect(sock, (struct sockaddr*)&peerAddr, peerAddrLen)) {
		perror("Failed to connect socket");
		abort();
	}

	// Returns port of connected client
	return htons(peerAddr.sin_port);
}



// Sends ATEM packet
void atem_socket_send(int sock, uint8_t* packet) {
	// Gets packet length from packet
	uint16_t len = atem_header_len_get(packet);

	if (logs_enable_send) {
		printf("Sending packet:\n");
		logs_print_buffer(stdout, packet, len);
	}

	// Sends packet to socket
	ssize_t sendLen = send(sock, packet, len, 0);
	if (sendLen == len) return;

	// Handles send failure
	if (sendLen == -1) {
		perror("Failed to send packet");
	}
	// Handles unexpected return value
	else {
		fprintf(stderr, "Failed to send: %zu (expected %d)\n", sendLen, len);
	}

	// Aborts running test
	abort();
}

// Receives ATEM packet
void atem_socket_recv(int sock, uint8_t* packet) {
	// Receives packet
	ssize_t recvLen = recv(sock, packet, ATEM_MAX_PACKET_LEN, 0);
	if (recvLen <= 0) {
		if (recvLen == -1) {
			perror("Failed to recv packet");
		}
		else if (recvLen == 0) {
			fprintf(stderr, "Received empty packet from client\n");
		}
		else {
			fprintf(stderr, "Failed to recv: %zu\n", recvLen);
		}
		abort();
	}

	// Verifies received ATEM packet
	atem_packet_verify(packet, recvLen);
}



// Ensures no more data is written when expecting session to be closed
void atem_socket_norecv(int sock) {
	if (simple_socket_select(sock, ATEM_TIMEOUT) == 0) return;

	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	fprintf(stderr, "Received data when expecting to time out\n");
	abort();
}
