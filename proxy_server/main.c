#include <stdio.h> // printf, perror, fprintf, stderr
#include <stdlib.h> // abort, exit, EXIT_FAILURE
#include <stdbool.h> // true

#include "./udp.h"
#include "./server.h"
#include "./timer.h"
#include "./relay.h"



// Entry point for software
int main(int argc, char** argv) {
	// Prints usage text for no command line argument
	if (argc < 2) {
		printf("Usage: %s <atem-ip-address>\n", argv[0]);
		return EXIT_FAILURE;
	}

	// Parses string address into an address type
	const in_addr_t atemAddr = inet_addr(argv[1]);
	if (atemAddr == (in_addr_t)(-1)) {
		fprintf(stderr, "Argument was not an IP address\n");
		return EXIT_FAILURE;
	}

	// Initializes windows networking
#ifdef _WIN32
	WSADATA wsaData;
	int wsaInitReturn = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsaInitReturn) {
		fprintf(stderr, "Failed to initialize WSA: %d\n", wsaInitReturn);
		return EXIT_FAILURE;
	}
#endif

	// Initializes proxy server and relay client
	if (!setupProxy()) {
		perror("Failed to initialize proxy server");
		abort();
	}
	if (!setupRelay()) {
		perror("Failed to initialize relay client");
		abort();
	}
	if (!relayEnable(atemAddr)) {
		perror("Failed to connect relay client to ATEM address");
		abort();
	}

	// Sets up for socket event handler
	int nfds = sockProxy;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockProxy, &fds);
	FD_SET(sockRelay, &fds);
	if (sockRelay > sockProxy) nfds = sockRelay;

	// Main event loop
	while (true) {
		// Gets next socket event
		fd_set fds_copy = fds;
		struct timeval* tv = timeToNextTimerEvent();
		int selectLen = select(nfds + 1, &fds_copy, NULL, NULL, tv);
		if (selectLen == -1) {
			perror("Select got an error");
			abort();
		}

		// Processes timer event when timer expired
		if (!selectLen) {
			timerEvent();
		}
		// Processes proxy server data
		else if (FD_ISSET(sockProxy, &fds_copy)) {
			processProxyData();
		}
		// Processes relay client data
		else if (FD_ISSET(sockRelay, &fds_copy)) {
			processRelayData();
		}
		// Should never occur
		else {
			fprintf(stderr, "Received socket data on unknown socket\n");
			abort();
		}
	}
}
