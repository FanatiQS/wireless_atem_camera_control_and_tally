#include <stdlib.h> // abort
#include <assert.h> // assert

#include "../utils/utils.h"

// Ensures client is connected and closes
void test_connected_and_close(int sock, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_remoteid_get_verify(packet, 0x0001);
	atem_header_ackid_get_verify(packet, 0x0000);
	atem_header_localid_get_verify(packet, 0x0000);

	atem_handshake_close(sock, sessionId);
	atem_socket_close(sock);
}

int main(void) {
	// Ensures successful opening handshake works as expected
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_connect(sock, 0x1337);
		test_connected_and_close(sock, sessionId);
	}

	// Ensures retransmitted opening handshake works as a normal opening handshake
	RUN_TEST() {
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		uint16_t clientSessionId = 0x1337;
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, true, clientSessionId);
		uint16_t newSessionId = atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_ACCEPT, false, clientSessionId);
		atem_acknowledge_response_send(sock, clientSessionId, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures server retransmits opening handshake accept response if not acknowledged
	RUN_TEST() {
		uint16_t clientSessionId = 0x1337;
		int sock = atem_socket_create();

		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);
		atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		atem_acknowledge_response_send(sock, clientSessionId, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures a bouble opening handshake response due to network latency is handled correctly
	RUN_TEST() {
		uint16_t clientSessionId = 0x1337;
		int sock = atem_socket_create();

		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);
		atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
		atem_acknowledge_response_send(sock, clientSessionId, 0x0000);
		atem_acknowledge_response_send(sock, clientSessionId, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures opening handshake accept response is resent ATEM_RESENDS times if not acknowledged and then closes session
	RUN_TEST() {
		uint16_t clientSessionId = 0x1337;
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



	// Ensures rejected opening handshake works as expected
	RUN_TEST() {
		int fillSock = atem_socket_create();
		for (int i = 0; atem_handshake_tryconnect(fillSock, i); i++);

		uint16_t sessionId = 0x1337;
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, sessionId);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
		atem_socket_close(fillSock);
	}

	// Ensures a dropped opening handshake reject response is resent without retransmit flag
	RUN_TEST() {
		int fillSock = atem_socket_create();
		for (int i = 0; atem_handshake_tryconnect(fillSock, i); i++);

		uint16_t sessionId = 0x1337;
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, sessionId);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, sessionId);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, true, sessionId);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
		atem_socket_close(fillSock);
	}

	return runner_exit();
}
