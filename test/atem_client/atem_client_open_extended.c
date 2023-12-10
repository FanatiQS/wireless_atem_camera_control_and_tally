#include <stdio.h> // perror
#include <stdlib.h> // abort
#include <sys/socket.h> // getpeername, socklen_t, struct sockaddr
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // ntohs

#include "../utils/utils.h"

// Gets port of ATEM servers client
uint16_t atem_port(int sock) {
	struct sockaddr_in sockAddr;
	socklen_t len = sizeof(sockAddr);
	if (getpeername(sock, (struct sockaddr *)&sockAddr, &len) == -1) {
		perror("Failed to call getsockname");
		abort();
	}
	return ntohs(sockAddr.sin_port);
}

int main(void) {
	// Ensures client resends opening handshake exactly ATEM_RESENDS times on an interval of ATEM_RESEND_TIME seconds
	RUN_TEST(
		atem_handshake_resetpeer();

		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_start_server(sock);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			struct timespec marker = timediff_mark();
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, sessionId);
			timediff_get_verify(marker, ATEM_RESEND_TIME, TIMEDIFF_LATE);
		}

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	);

	// Ensures opening handshake restarts correctly after ATEM_RESENDS failed retries with new session id and port
	RUN_TEST(
		atem_handshake_resetpeer();

		// Gets client assigned session id and peer port on first connection attempt
		int sock = atem_socket_create();
		uint16_t sessionId1 = atem_handshake_start_server(sock);
		uint16_t port1 = atem_port(sock);
		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, sessionId1);
		}
		atem_socket_close(sock);

		// Opening handshake client assigned session id should have changed
		sock = atem_socket_create();
		struct timespec marker = timediff_mark();
		uint16_t sessionId2 = atem_handshake_start_server(sock);
		timediff_get_verify(marker, 450, 50);
		uint16_t port2 = atem_port(sock);
		atem_socket_close(sock);

		// Ensures session id and port change between connections
		if (sessionId2 == sessionId1) {
			fprintf(stderr,
				"Expected session id after failed opening handshake reconnects to change: %04x, %04x\n",
				sessionId1,
				sessionId2
			);
			abort();
		}
		if (port1 == port2) {
			fprintf(stderr, "Expected UDP client port to change: %d, %d\n", port1, port2);
			abort();
		}
	);

	// Ensures restarting opening handshake after reject changes session id and port 
	RUN_TEST(
		atem_handshake_resetpeer();

		int sock = atem_socket_create();
		uint16_t sessionId1 = atem_handshake_start_server(sock);
		uint16_t port1 = atem_port(sock);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId1);
		atem_socket_close(sock);

		sock = atem_socket_create();
		struct timespec marker = timediff_mark();
		uint16_t sessionId2 = atem_handshake_start_server(sock);
		timediff_get_verify(marker, 270, 10);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId1);
		uint16_t port2 = atem_port(sock);
		atem_socket_close(sock);

		if (sessionId1 == sessionId2) {
			fprintf(stderr,
				"Expected session id on reconnect after reject to be different than when rejected: %04x, %04x\n",
				sessionId1,
				sessionId2
			);
		}
		if (port1 == port2) {
			fprintf(stderr, "Expected UDP client port to change: %d, %d", port1, port2);
			abort();
		}
	);
}
