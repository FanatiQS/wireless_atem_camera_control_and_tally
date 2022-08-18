#include <stdio.h> // perror
#include <stdlib.h> // abort

#include "./udp.h"
#include "../src/atem.h"

// Creates a UDP socket
int createSocket() {
	const int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		perror("Failed to create socket");
		abort();
	}
	return sock;
}

// Creates a socket address struct with address and ATEM port
struct sockaddr_in createAddr(const in_addr_t addr) {
	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = addr
	};
	return sockAddr;
}
