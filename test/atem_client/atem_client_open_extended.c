#include <stdio.h> // perror
#include <stdlib.h> // abort

#include <arpa/inet.h> // ntohs

#include "../utils/utils.h"

int main(void) {
	// Ensures client resends opening handshake exactly ATEM_RESENDS times on a ATEM_RESEND_TIME ms interval
	RUN_TEST() {
		// Gets initial opening handshake request from client
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_server(sock);

		// Measures opening handshake request retransmit interval ATEM_RESENDS times
		for (int i = 0; i < ATEM_RESENDS; i++) {
			struct timespec marker = timediff_mark();
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, session_id);
			timediff_get_verify(marker, ATEM_RESEND_TIME, TIMEDIFF_LATE);
		}

		// Client should switch UDP port and session id
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures opening handshake restarts correctly after ATEM_RESENDS failed retries with new session id and port
	RUN_TEST() {
		uint8_t packet[ATEM_PACKET_LEN_MAX];

		// Gets client assigned session id and peer port on first connection attempt
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t port1 = ntohs(atem_socket_listen(sock, packet).sin_port);
		uint16_t session_id1 = atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);
		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, session_id1);
		}
		atem_socket_close(sock);

		// Gets new opening handshake request and validates time before retry
		sock = atem_socket_create();
		struct timespec marker = timediff_mark();
		uint16_t port2 = ntohs(atem_socket_listen(sock, packet).sin_port);
		timediff_get_verify(marker, 450, 50);
		uint16_t session_id2 = atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);
		atem_socket_close(sock);

		// Ensures session id and port change on restart
		if (session_id2 == session_id1) {
			fprintf(stderr,
				"Expected session id after failed opening handshake reconnects to change: %04x, %04x\n",
				session_id1,
				session_id2
			);
			abort();
		}
		if (port1 == port2) {
			fprintf(stderr, "Expected UDP client port to change: %d, %d\n", port1, port2);
			abort();
		}
	}

	// Ensures restarting opening handshake after reject changes session id and port
	RUN_TEST() {
		uint8_t packet[ATEM_PACKET_LEN_MAX];

		// Gets client assigned session id and peer port on first connection attempt
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t port1 = ntohs(atem_socket_listen(sock, packet).sin_port);
		uint16_t session_id1 = atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, session_id1);
		atem_socket_close(sock);

		// Gets new opening handshake request and validates time before restart
		sock = atem_socket_create();
		struct timespec marker = timediff_mark();
		uint16_t port2 = ntohs(atem_socket_listen(sock, packet).sin_port);
		timediff_get_verify(marker, 270, 20);
		uint16_t session_id2 = atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);
		atem_socket_close(sock);

		// Ensures session id and port changed on restart
		if (session_id1 == session_id2) {
			fprintf(stderr,
				"Expected session id on reconnect after reject to be different than when rejected: %04x, %04x\n",
				session_id1,
				session_id2
			);
		}
		if (port1 == port2) {
			fprintf(stderr, "Expected UDP client port to change: %d, %d", port1, port2);
			abort();
		}
	}

	return runner_exit();
}
