#include "../utils/utils.h"

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

	// Ensures client sending closing request after server has already started a closing handshake is handled properly
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

	// Ensures client initiated closing request closes the session correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_connect(sock, 0x1234);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		uint8_t packet[ATEM_MAX_PACKET_LEN];
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
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, sessionId);

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
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		do {
			atem_socket_recv(sock, packet);
		} while (atem_handshake_opcode_get(packet) != ATEM_OPCODE_CLOSED);
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	return runner_exit();
}
