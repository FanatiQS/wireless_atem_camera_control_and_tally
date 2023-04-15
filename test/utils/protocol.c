#include <stdint.h> // uint8_t, uint16_t
#include <stdio.h> // perror
#include <string.h> // memset

#include <unistd.h> // close
#include <sys/socket.h> // socket, AF_INET, SOCK_DGRAM, struct timeval, setsockopt, SOL_SOCKET SO_RCVTIMEO, SO_SNDTIMEO, ssize_t send, recv, bind, connect, struct sockaddr
#include <arpa/inet.h> // struct sockaddr_in, htons, inet_addr
#include <sys/select.h> // select, FD_ZERO, FD_SET, fd_set

#include "../../src/atem_private.h" // ATEM_FLAG_ACK, ATEM_LEN_ACK, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW, ATEM_FLAG_SYN, ATEM_FLAG_RETX
#include "../../src/atem.h" // ATEM_MAX_PACKET_LEN
#include "./header.h" // atem_header_len_get, atem_header_len_verify, atem_header_flags_set, atem_header_flags_get, atem_header_len_set, atem_packet_word_get
#include "./runner.h" // testrunner_abort
#include "./logs.h" // print_buffer, print_debug



#define TIMEOUT_LISTEN 10
#define TIMEOUT_WAIT 2



// The ipv4 address to the server to test
char* atemServerAddr;



// Zeroes out packet data
void atem_packet_clear(uint8_t* packet) {
	memset(packet, 0, ATEM_MAX_PACKET_LEN);
}

// Verifies ATEM packet length and unknown id
void atem_packet_verify(uint8_t* packet, ssize_t recvLen) {
	print_buffer("Received packet", packet, recvLen);

	// Verifies unknown id
	int checkUnknownId = 0;
	if (checkUnknownId) {
		uint8_t flags = atem_header_flags_get(packet);
		uint8_t unknownId = atem_packet_word_get(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW);
		if (flags == ATEM_FLAG_SYN) {
			if (unknownId != 0x003a) {
				print_debug("uknown id 0x3a\n");
				testrunner_abort();
			}
		}
		else if (flags == (ATEM_FLAG_SYN | ATEM_FLAG_RETX)) {
			if (unknownId != 0x00cd) {
				print_debug("uknown id 0xcd\n");
				testrunner_abort();
			}
		}
		else {
			if (unknownId != 0x0000) {
				print_debug("uknown id 0x0000\n");
				testrunner_abort();
			}
		}
	}

	// Verifies packet length
	atem_header_len_verify(packet, recvLen);
}



// Initializes UDP socket to ATEM switcher or client
int atem_socket_init() {
	// Creates socket
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("Failed to create socket");
		testrunner_abort();
	}

	// Sets socket to timeout if failing to transmit or receive data
	struct timeval timer = { .tv_sec = TIMEOUT_WAIT };
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timer, sizeof(timer))) {
		perror("Failed to setsockopt SORCVTIMEO");
		testrunner_abort();
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timer, sizeof(timer))) {
		perror("Failed to setsockopt SO_SNDTIMEO");
		testrunner_abort();
	}

	return sock;
}

// Closes socket to ATEM switcher or client
void atem_socket_close(int sock) {
	close(sock);
}



// Connects client socket to ATEM server address
void atem_socket_connect(int sock) {
	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = inet_addr(atemServerAddr)
	};
	if (connect(sock, (const struct sockaddr*)&addr, sizeof(addr))) {
		perror("Failed to connect socket");
		testrunner_abort();
	}
}

// Listens for an ATEM client to connect
uint16_t atem_socket_listen(int sock, uint8_t* packet) {
	// Binds socket to ATEM client address for receiving packets
	struct sockaddr_in bindAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = INADDR_ANY
	};
	if (bind(sock, (const struct sockaddr*)&bindAddr, sizeof(bindAddr))) {
		perror("Failed to bind socket");
		testrunner_abort();
	}

	// Awaits first packet from client
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	struct timeval timer = { .tv_sec = TIMEOUT_LISTEN };
	int selectLen = select(sock + 1, &fds, NULL, NULL, NULL);
	if (selectLen == -1) {
		perror("Select got an error");
		testrunner_abort();
	}
	if (selectLen != 1) {
		print_debug("Received unexpected select value: %d\n", selectLen);
		testrunner_abort();
	}

	// Receives first packet
	struct sockaddr_in peerAddr;
	socklen_t peerAddrLen = sizeof(peerAddr);
	ssize_t recvLen = recvfrom(sock, packet, ATEM_MAX_PACKET_LEN, 0, (struct sockaddr*)&peerAddr, &peerAddrLen);
	if (recvLen < 0) {
		if (recvLen == -1) {
			perror("Failed to recvfrom packet");
		}
		else {
			print_debug("Function call 'recvfrom' failed: %zu\n", recvLen);
		}
	}

	// Verifies received ATEM packet
	atem_packet_verify(packet, recvLen);

	// Connects socket to ATEM client for outgoing packets
	if (connect(sock, (struct sockaddr*)&peerAddr, peerAddrLen)) {
		perror("Failed to connect socket");
		testrunner_abort();
	}

	// Returns port of connected client
	return htons(peerAddr.sin_port);
}

// Listens for a fresh ATEM client to connect
int atem_socket_listen_fresh(uint8_t* packet) {
	// Gets socket already trying to connect
	int sock = atem_socket_init();
	int port = atem_socket_listen(sock, packet);

	// Flushes all requests made to already trying socket to process request from initial packet
	do {
		atem_socket_close(sock);
		sock = atem_socket_init();
	} while (atem_socket_listen(sock, packet) == port);

	return sock;
}



// Sends ATEM packet
void atem_socket_send(int sock, uint8_t* packet) {
	// Gets packet length from packet
	uint16_t len = atem_header_len_get(packet);

	// Prints packet
	print_buffer("Sending packet", packet, len);

	// Sends packet to socket
	ssize_t sendLen = send(sock, packet, len, 0);
	if (sendLen == len) return;

	// Handles send failure
	if (sendLen == -1) {
		perror("Failed to send packet");
	}
	// Handles unexpected return value
	else {
		print_debug("Function call 'send' failed: %zu (expected %d)\n", sendLen, len);
	}

	// Aborts running test
	testrunner_abort();
}

// Receives ATEM packet
void atem_socket_recv(int sock, uint8_t* packet) {
	// Receives packet
	ssize_t recvLen = recv(sock, packet, ATEM_MAX_PACKET_LEN, 0);
	if (recvLen < 0) {
		if (recvLen == -1) {
			perror("Failed to recv packet");
		}
		else {
			print_debug("Function call 'recv' failed: %zu\n", recvLen);
		}
		testrunner_abort();
	}

	// Verifies received ATEM packet
	atem_packet_verify(packet, recvLen);
}



// Ensures no more data is written when expecting session to be closed
void atem_socket_norecv(int sock) {
	struct fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	struct timeval timer = { .tv_sec = ATEM_TIMEOUT };
	int selectLen = select(sock + 1, &fds, NULL, NULL, &timer);

	if (selectLen == 0) return;
	
	if (selectLen == -1) {
		perror("Select got an error");
	}
	else {
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		atem_socket_recv(sock, packet);
		print_debug("Received data when execting not to\n");
	}

	testrunner_abort();
}
