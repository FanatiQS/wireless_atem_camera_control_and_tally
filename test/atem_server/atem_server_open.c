#include <stdlib.h> // abort
#include <assert.h> // assert

#include "../utils/utils.h"

// Ensures client is connected and closes
static void test_connected_and_close(int sock, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK);
	atem_header_sessionid_get_verify(packet, session_id);
	atem_header_remoteid_get_verify(packet, 0x0001);
	atem_header_ackid_get_verify(packet, 0x0000);
	atem_header_localid_get_verify(packet, 0x0000);

	atem_handshake_close(sock, session_id);
	atem_socket_close(sock);
}

int main(void) {
	// Ensures successful opening handshake works as expected
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, 0x1337);
		test_connected_and_close(sock, session_id);
	}

	// Ensures retransmitted opening handshake works as a normal opening handshake
	RUN_TEST() {
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		uint16_t session_id_client = 0x1337;
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, true, session_id_client);
		uint16_t newSessionId = atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_ACCEPT, false, session_id_client);
		atem_acknowledge_response_send(sock, session_id_client, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures server retransmits opening handshake accept response if not acknowledged
	RUN_TEST() {
		uint16_t session_id_client = 0x1337;
		int sock = atem_socket_create();

		uint16_t newSessionId = atem_handshake_start_client(sock, session_id_client);
		atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		atem_acknowledge_response_send(sock, session_id_client, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures a double opening handshake response due to network latency is handled correctly
	RUN_TEST() {
		uint16_t session_id_client = 0x1337;
		int sock = atem_socket_create();

		uint16_t newSessionId = atem_handshake_start_client(sock, session_id_client);
		atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		atem_acknowledge_response_send(sock, session_id_client, 0x0000);
		atem_acknowledge_response_send(sock, session_id_client, 0x0000);

		test_connected_and_close(sock, newSessionId | 0x8000);
	}

	// Ensures opening handshake accept response is resent ATEM_RESENDS times if not acknowledged and then closes session
	RUN_TEST() {
		uint16_t session_id_client = 0x1337;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, session_id_client);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures delayed opening handshake response acknowledgement arriving after closing request is sent is ignored
	RUN_TEST() {
		uint16_t session_id_client = 0x1337;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, session_id_client);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_acknowledge_response_send(sock, serverSessionId, 0x0000);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, true, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures opening handshake response from retransmitted request does not consume from retransmit count
	RUN_TEST() {
		uint16_t session_id_client = 0x1337;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, session_id_client);

		// Retransmits opening handshake and reads response 50 times
		for (int i = 0; i < 50; i++) {
			atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, true, session_id_client);
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		}

		// Server should still retransmit response ATEM_RESENDS times even though it has already been retransmitted from the retransmitted requests
		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, newSessionId);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}



	// Ensures rejected opening handshake works as expected
	RUN_TEST() {
		int sock_fill = atem_socket_create();
		atem_handshake_fill(sock_fill);

		uint16_t session_id = 0x1337;
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, session_id);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, session_id);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
		atem_socket_close(sock_fill);
	}

	// Ensures a dropped opening handshake reject response is resent without retransmit flag
	RUN_TEST() {
		int sock_fill = atem_socket_create();
		atem_handshake_fill(sock_fill);

		uint16_t session_id = 0x1337;
		int sock = atem_socket_create();
		atem_socket_connect(sock);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, session_id);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, session_id);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, true, session_id);
		atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_REJECT, false, session_id);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
		atem_socket_close(sock_fill);
	}

	// Ensures new client can connect after 1 client disconnects from a full server
	RUN_TEST() {
		int sock_fill = atem_socket_create();
		uint16_t session_id_first = atem_handshake_connect(sock_fill, 0x0022);
		atem_handshake_fill(sock_fill);

		int sock = atem_socket_create();
		atem_socket_connect(sock);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, 0x1234);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_REJECT, false, 0x1234);

		atem_handshake_close(sock_fill, session_id_first);

		uint16_t session_id_last = atem_handshake_connect(sock, 0x5678);
		atem_handshake_close(sock, session_id_last);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
		atem_socket_close(sock_fill);
	}

	// Out of order opening handshake, acknowledging the second opened session before the first
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id1 = atem_handshake_start_client(sock, 0x1234);
		uint16_t session_id2 = atem_handshake_start_client(sock, 0x5678);
		atem_acknowledge_response_send(sock, 0x5678, 0x0000);

		atem_acknowledge_response_send(sock, 0x1234, 0x0000);
		atem_handshake_close(sock, session_id1 | 0x8000);
		atem_handshake_close(sock, session_id2 | 0x8000);
	}



	// Ensures session id can not be hijacked from another socket
	RUN_TEST() {
		uint16_t session_id_client = 0x1234;

		int sock1 = atem_socket_create();
		atem_socket_connect(sock1);

		int sock2 = atem_socket_create();
		atem_socket_connect(sock2);

		atem_handshake_sessionid_send(sock1, ATEM_OPCODE_OPEN, false, session_id_client);
		atem_handshake_newsessionid_recv(sock1, ATEM_OPCODE_ACCEPT, false, session_id_client);
		atem_acknowledge_response_send(sock2, session_id_client, 0x0000);
		atem_handshake_newsessionid_recv(sock1, ATEM_OPCODE_ACCEPT, true, session_id_client);

		atem_socket_norecv(sock2);
		atem_socket_close(sock1);
		atem_socket_close(sock2);
	}

	return runner_exit();
}
