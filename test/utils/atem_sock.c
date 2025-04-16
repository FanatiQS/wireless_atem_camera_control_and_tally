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
void atem_packet_verify(uint8_t* packet, size_t recv_len) {
	if (logs_find("atem_recv")) {
		printf("Received packet:\n");
		logs_print_buffer(stdout, packet, recv_len);
	}

	// Verifies packet length
	atem_header_len_get_verify(packet, recv_len);

	// Verifies that the correct IDs are set or not set for flags
	uint8_t flags = atem_header_flags_get(packet);
	if (flags & ATEM_FLAG_SYN) {
		atem_header_flags_get_verify(packet, ATEM_FLAG_SYN, ATEM_FLAG_RETX);
		atem_header_len_get_verify(packet, ATEM_LEN_SYN);
	}
	if (!(flags & ATEM_FLAG_ACK)) {
		atem_header_ackid_get_verify(packet, 0x0000);
	}
	if (!(flags & ATEM_FLAG_ACKREQ)) {
		atem_header_remoteid_get_verify(packet, 0x0000);
	}
	if (!(flags & ATEM_FLAG_RETXREQ)) {
		atem_header_localid_get_verify(packet, 0x0000);
	}

	// Verifies unknown id
	int checkUnknownId = 0;
	if (checkUnknownId) {
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
	const char* env_key = "ATEM_CLIENT_ADDR";
	const char* env_value = getenv(env_key);
	if (env_value == NULL) {
		fprintf(stderr, "Environment variable %s not defined\n", env_key);
		abort();
	}
	const in_addr_t cmp_addr = inet_addr(env_value);

	struct timespec timeout_start = timediff_mark();
	struct sockaddr_in peer_addr;
	ssize_t recv_len;
	do {
		// Awaits first packet from ATEM client
		if (simple_socket_poll(sock, TIMEOUT_LISTEN - timediff_get(timeout_start)) != 1) {
			fprintf(stderr, "No client connected\n");
			abort();
		}

		// Receives first packet
		socklen_t peer_addr_len = sizeof(peer_addr);
		recv_len = recvfrom(sock, packet, ATEM_PACKET_LEN_MAX, 0, (struct sockaddr*)&peer_addr, &peer_addr_len);
		assert(peer_addr_len == sizeof(peer_addr));
		if (recv_len <= 0) {
			if (recv_len == -1) {
				perror("Failed to recvfrom packet");
			}
			else if (recv_len == 0) {
				fprintf(stderr, "Received empty packet from client\n");
			}
			else {
				fprintf(stderr, "Failed to recvfrom: %zu\n", recv_len);
			}
			abort();
		}
	} while (peer_addr.sin_addr.s_addr != cmp_addr);

	// Verifies received ATEM packet
	atem_packet_verify(packet, (size_t)recv_len);

	// Connects socket to ATEM client
	if (connect(sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr))) {
		perror("Failed to connect socket");
		abort();
	}

	// Returns connection information for connected client
	return peer_addr;
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
	size_t recv_len = simple_socket_recv(sock, packet, ATEM_PACKET_LEN_MAX);
	if (recv_len < ATEM_LEN_HEADER) {
		fprintf(stderr, "Received packet from client smaller than ATEM header\n");
		abort();
	}

	// Verifies received ATEM packet
	atem_packet_verify(packet, recv_len);
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
