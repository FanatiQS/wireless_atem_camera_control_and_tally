#include "../utils/utils.h"

int main(void) {
	// Ensures an unanswered SYN packet is retransmitted at least ATEM_RESENDS times
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_start_server(sock);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, sessionId);
		}

		atem_socket_close(sock);
	}



	// Ensures a successful opening handshake response connects client
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t clientSessionId = atem_handshake_start_server(sock);

		uint16_t newSessionId = 0x0001;
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, clientSessionId, newSessionId);
		atem_acknowledge_response_recv_verify(sock, clientSessionId, 0x0000);

		// Ensures connected by sending ping
		uint16_t serverSessionId = newSessionId | 0x8000;
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, serverSessionId, remoteId);
		atem_acknowledge_response_recv_verify(sock, serverSessionId, remoteId);

		atem_handshake_close(sock, serverSessionId);
		atem_socket_close(sock);
	}

	// Ensures a resent opening handshake also successful opens
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t clientSessionId = atem_handshake_start_server(sock);

		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, clientSessionId);

		uint16_t newSessionId = 0x0001;
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, clientSessionId, newSessionId);
		atem_acknowledge_response_recv_verify(sock, clientSessionId, 0x0000);

		// Ensures connected by sending ping
		uint16_t serverSessionId = newSessionId | 0x8000;
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, serverSessionId, remoteId);
		atem_acknowledge_response_recv_verify(sock, serverSessionId, remoteId);

		atem_handshake_close(sock, serverSessionId);
		atem_socket_close(sock);
	}

	// Ensures a resent accept is also responded to
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t clientSessionId = atem_handshake_start_server(sock);

		uint16_t newSessionId = 0x0001;
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, clientSessionId, newSessionId);
		atem_acknowledge_response_recv_verify(sock, clientSessionId, 0x0000);

		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		atem_acknowledge_response_recv_verify(sock, clientSessionId, 0x0000);

		// Ensures connected by sending ping
		uint16_t serverSessionId = newSessionId | 0x8000;
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, serverSessionId, remoteId);
		atem_acknowledge_response_recv_verify(sock, serverSessionId, remoteId);

		atem_handshake_close(sock, serverSessionId);
		atem_socket_close(sock);
	}

	// Ensures resent opening handshake response is acknowledged if initial response was dropped
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t clientSessionId = atem_handshake_start_server(sock);

		uint16_t newSessionId = 0x0001;
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		atem_acknowledge_response_recv_verify(sock, clientSessionId, 0x0000);

		// Ensures connected by sending ping
		uint16_t serverSessionId = newSessionId | 0x8000;
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, serverSessionId, remoteId);
		atem_acknowledge_response_recv_verify(sock, serverSessionId, remoteId);

		atem_handshake_close(sock, serverSessionId);
		atem_socket_close(sock);
	}



	// Ensures no messages are sent after responding to client with a reject response
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_start_server(sock);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a reject response to a resent open request is handled as a normal reject
	RUN_TEST() {
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_start_server(sock);

		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, sessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a rejected ATEM client retries connecting and does not give up retrying
	RUN_TEST() {
		atem_handshake_resetpeer();
		for (int i = 0; i < ATEM_RESENDS + 2; i++) {
			int sock = atem_socket_create();
			uint16_t sessionId = atem_handshake_start_server(sock);

			atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId);

			atem_socket_close(sock);
		}
	}

	return runner_exit();
}
