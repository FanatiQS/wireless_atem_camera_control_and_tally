#include "../utils/utils.h"

// Tests that an unanswered SYN packet is resent at least ATEM_RESENDS times
void atem_client_open_retry() {
	int sock = atem_socket_create();
	atem_handshake_start_server(sock);
	atem_socket_close(sock);

	for (int i = 0; i < ATEM_RESENDS; i++) {
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		int sock = atem_socket_create();
		atem_socket_listen(sock, packet);
		uint16_t sessionId = atem_header_sessionid_get(packet);
		atem_handshake_common_get_verify(packet, ATEM_OPCODE_OPEN, true, sessionId);
		atem_socket_close(sock);
	}
}

// Tests that a rejected ATEM client retries connecting and does not give up retrying
void atem_client_open_reject() {
	for (int i = 0; i < ATEM_RESENDS + 2; i++) {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_start_server(sock);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId);
		atem_socket_close(sock);
	}
}

// Tests that a successful SYN/ACK response is acknowledged
void atem_client_open_accept() {
	int sock = atem_socket_create();
	uint16_t newSessionId = atem_handshake_listen(sock, 0x0001);
	atem_handshake_close(sock, newSessionId);
	atem_socket_close(sock);
}

// Tests that a resent accept is also responded to
void atem_client_open_acceptResend() {
	int sock = atem_socket_create();

	uint16_t newSessionId = 0x0001;
	uint16_t sessionId = atem_handshake_start_server(sock);
	atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, sessionId, newSessionId);
	atem_ack_recv_verify(sock, sessionId, 0x0000);

	atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, true, sessionId, newSessionId);
	atem_ack_recv_verify(sock, sessionId, 0x0000);
}



// Tests that a client resends the SYN packet only 10 times if not answered and with about 200ms between each resend
void atem_client_open_retryInterval() {
	// ATEM client starts connecting
	int sock = atem_socket_create();
	uint16_t sessionId = atem_handshake_start_server(sock);

	// Lets it retry connecting 10 times
	struct timespec start = timer_start();
	for (int i = 0; i < ATEM_RESENDS; i++) {
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, sessionId);
	}
	uint32_t diff = timer_end(start);

	// No more data should arrive from the client
	atem_socket_norecv(sock);
	atem_socket_close(sock);

	// Avarage time between resends should be 200ms
	uint32_t timeBetweenPackets = diff / 10;
	uint32_t roundedTimeBetweenPackets = timeBetweenPackets / 10 * 10;
	if (roundedTimeBetweenPackets != ATEM_RESEND_TIME) {
		print_debug("Expected time between packets to be %ums, but got %ums\n", ATEM_RESEND_TIME, roundedTimeBetweenPackets);
		testrunner_abort();
	}
}

// Tests that client regenerates the client assigned session id after restarting failed connection
void atem_client_open_randomSessionId() {
	int sock = atem_socket_create();
	uint16_t sessionId = atem_handshake_start_server(sock);
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, sessionId);
	atem_socket_close(sock);

	sock = atem_socket_create();
	uint16_t reconnectSessionId = atem_handshake_start_server(sock);
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, reconnectSessionId);
	atem_socket_close(sock);

	if (reconnectSessionId == sessionId) {
		print_debug("Session ID did not change as expected: 0x%04x(old), 0x%04x(new)\n", sessionId, reconnectSessionId);
		testrunner_abort();
	}
}



// Runs tests that validate a client would work with an ATEM switcher
void atem_client_open_minimal() {
	TESTRUNNER(atem_client_open_reject);
	TESTRUNNER(atem_client_open_retry);
	TESTRUNNER(atem_client_open_accept);
	TESTRUNNER(atem_client_open_acceptResend);
}

// Runs optional tests validating a client would be indistinguishable from ATEM Software Control
void atem_client_open_optional() {
	TESTRUNNER(atem_client_open_retryInterval);
	TESTRUNNER(atem_client_open_randomSessionId);
}

// Runs tests that validates a clients response to invalid ATEM protocol packets
void atem_client_invalid() {
	// @todo no tests here yet
}
