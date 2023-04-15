#include "../utils/utils.h"

// Tests that the first packet from a client is a non resent SYN packet
void atem_client_base_open_syn() {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	int sock = atem_socket_listen_fresh(packet);
	atem_handshake_opcode_verify(packet, ATEM_OPCODE_OPEN);
	atem_header_flags_isnotset(packet, ATEM_FLAG_RETX);
	atem_socket_close(sock);
}

// Tests that a client resends the SYN packet 10 times if not answered with about 200ms between each resend
void atem_client_strict_open_numberOfResends() {
	// Awaits client to make a fresh connection
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	int sock = atem_socket_listen_fresh(packet);
	atem_handshake_opcode_verify(packet, ATEM_OPCODE_OPEN);
	atem_header_flags_isnotset(packet, ATEM_FLAG_RETX);
	uint16_t sessionId = atem_header_sessionid_get(packet);

	// Lets it retry connecting 10 times
	struct timespec start = timer_start();
	for (int i = 0; i < ATEM_RESENDS; i++) {
		atem_socket_recv(sock, packet);
		atem_handshake_verify(packet, ATEM_OPCODE_OPEN, true, sessionId);
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
void atem_client_strict_open_randomSessionId() {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_close(atem_socket_listen_fresh(packet));
	uint16_t sessionId = atem_header_sessionid_get(packet);
	atem_socket_close(atem_socket_listen_fresh(packet));
	uint16_t reconnectSessionId = atem_header_sessionid_get(packet);
	if (reconnectSessionId == sessionId) {
		print_debug("Session ID did not change as expected: 0x%04x(old), 0x%04x(new)\n", sessionId, reconnectSessionId);
		testrunner_abort();
	}
}



// Runs tests that validate a client would work with an ATEM switcher
void atem_client_base_open() {
	TESTRUNNER(atem_client_base_open_syn);
}

// Runs tests that validate a client would be indistinguishable from ATEM Software Control
void atem_client_strict_open() {
	TESTRUNNER(atem_client_strict_open_numberOfResends);
	TESTRUNNER(atem_client_strict_open_randomSessionId);
}

// Runs tests that validates a clients response to invalid ATEM protocol packets
void atem_client_invalid() {
	// @todo no tests here yet
}
