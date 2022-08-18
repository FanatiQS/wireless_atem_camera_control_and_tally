#include <stdio.h> // perror, fprintf, stderr
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
		exit(EXIT_FAILURE);
	}

	// Initializes proxy server and relay client
	setupProxy();
	setupRelay(argv[1]);

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

