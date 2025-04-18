#include "../utils/utils.h"

// Connects to ATEM server and waits for server to start closing handshake
static uint16_t test_connect_and_drop(int sock) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];

	// Connects to ATEM server
	uint16_t session_id = atem_handshake_connect(sock, 0x1234);

	// Acknowledges ATEM state dump
	do {
		atem_acknowledge_keepalive(sock, packet);
		atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK);
		atem_header_sessionid_get_verify(packet, session_id);
		atem_header_ackid_get_verify(packet, 0x0000);
		atem_header_localid_get_verify(packet, 0x0000);
	} while (atem_header_len_get(packet) > ATEM_LEN_HEADER);

	// Does not acknowledge pings after state dump to force server initiated closing handshake
	while (1) {
		atem_socket_recv(sock, packet);
		if (atem_header_flags_get(packet) & ATEM_FLAG_SYN) {
			atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);
			break;
		}
		atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK | ATEM_FLAG_RETX);
		atem_header_sessionid_get_verify(packet, session_id);
		atem_header_ackid_get_verify(packet, 0x0000);
		atem_header_localid_get_verify(packet, 0x0000);
	}
	atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);

	return session_id;
}

int main(void) {
	// Ensures server closes session if not acknowledging opening handshake response
	RUN_TEST() {
		uint16_t clientSessionId = 0x1234;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures server resends closing request only once after client not acknowledging opening handshake response
	RUN_TEST() {
		uint16_t clientSessionId = 0x1234;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, true, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client and server starting closing handshake at the same time during opening handshake is handled properly
	RUN_TEST() {
		uint16_t clientSessionId = 0x1234;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures initiating closing handshake before opening handshake has completed works
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_client(sock, 0x1234);
		atem_handshake_close(sock, session_id | 0x8000);
		atem_socket_close(sock);
	}



	// Ensures server closes session if not acknowledging pings
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = test_connect_and_drop(sock);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures server resends closing request only once after client not acknowledging pings
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = test_connect_and_drop(sock);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, true, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client and server starting closing handshake at the same time after opening handshake is handled properly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = test_connect_and_drop(sock);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures server starts closing handshake within expected time frame
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, 0x4334);

		struct timespec timeout_start = timediff_mark();
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (!(atem_header_flags_get(packet) & ATEM_FLAG_SYN));

		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);
		timediff_get_verify(timeout_start, 0, ATEM_TIMEOUT_MS);
	}



	// Ensures client initiated closing request closes the session correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_connect(sock, 0x1234);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures retransmitted client initiated closing request also closes the session correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_connect(sock, 0x1234);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, true, sessionId);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures retransmitted closing request is not processed if previous closing closed the session
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t  session_id = atem_handshake_connect(sock, 0x1234);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, session_id);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (!(atem_header_flags_get(packet) & ATEM_FLAG_SYN));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, session_id);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, true, session_id);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}



	// Ensures client closing session before acknowledging opening handshake response closes session correctly
	RUN_TEST() {
		uint16_t clientSessionId = 0x1234;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);
		uint16_t serverSessionId = newSessionId | 0x8000;

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (atem_handshake_opcode_get(packet) != ATEM_OPCODE_CLOSED);
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures closing connected session before another one has connected works correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id_a = atem_handshake_connect(sock, 0x1234);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, 0x5678);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (!(atem_header_flags_get(packet) & ATEM_FLAG_SYN));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_ACCEPT, false, 0x5678);
		uint16_t session_id_b = atem_handshake_newsessionid_get(packet) | 0x8000;

		atem_handshake_close(sock, session_id_a);
		atem_handshake_close(sock, session_id_b);
	}



	// Ensures out of order closing handshake works correctly (closes first connection first and second after)
	RUN_TEST() {
		int sock1 = atem_socket_create();
		int sock2 = atem_socket_create();
		uint16_t session_id1 = atem_handshake_connect(sock1, 0x1234);
		uint16_t session_id2 = atem_handshake_connect(sock2, 0x5678);
		atem_handshake_close(sock1, session_id1);
		atem_handshake_close(sock2, session_id2);
		atem_socket_close(sock1);
		atem_socket_close(sock2);
	}

	// Ensures closing response to never sent request is ignored correctly while session is connected
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, 0x0001);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, session_id);

		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (atem_header_flags_get(packet) & ATEM_FLAG_ACKREQ);
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures closing response to never sent request is ignored correctly during opening handshake
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_client(sock, session_id) | 0x8000;
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, session_id);

		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
			atem_header_flags_get(packet);
		} while (atem_handshake_opcode_get(packet) & ATEM_OPCODE_ACCEPT);
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Drops multiple sessions at the same time with one remaining connected
	RUN_TEST() {
		uint8_t packet[ATEM_PACKET_LEN_MAX];

		// Connects session and fills remaining session slots
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, 0x0001);
		uint16_t fill_count = atem_handshake_fill(sock);

		// Acknowledges all packets from first session but drops all filler sessions packets until disconnected
		while (fill_count > 0) {
			atem_socket_recv(sock, packet);
			if (atem_header_sessionid_get(packet) == session_id) {
				atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK | ATEM_FLAG_RETX);
				atem_acknowledge_response_send(sock, session_id, atem_header_remoteid_get(packet));
			}
			else if (atem_header_flags_get(packet) & ATEM_FLAG_SYN) {
				atem_handshake_opcode_get_verify(packet, ATEM_OPCODE_CLOSING);
				atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, atem_header_sessionid_get(packet));
				fill_count--;
			}
		}

		// First session should still be connected
		atem_acknowledge_keepalive(sock, packet);
		atem_header_sessionid_get_verify(packet, session_id);

		atem_handshake_close(sock, session_id);
		atem_socket_norecv(sock);
	}

	return runner_exit();
}
