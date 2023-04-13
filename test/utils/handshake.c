#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool

#include "../../src/atem_private.h" // ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW, ATEM_FLAG_SYN, ATEM_LEN_SYN, ATEM_INDEX_OPCODE, ATEM_FLAG_RETX, ATEM_CONNECTION_OPEN
#include "../../src/atem.h" // ATEM_CONNECTION_SUCCESS, ATEM_CONNECTION_REJECTED, ATEM_CONNECTION_CLOSING, ATEM_CONNECTION_CLOSED, ATEM_MAX_PACKET_LEN
#include "./header.h" // atem_packet_word_set, atem_packet_word_get, atem_header_flags_set, atem_header_flags_verify, atem_header_len_set, atem_header_len_verify, atem_header_sessionid_get, atem_header_ackid_verify, atem_header_localid_verify, atem_header_remoteid_verify, atem_header_sessionid_set, atem_header_sessionid_verify
#include "./runner.h" // testrunner_abort
#include "./protocol.h" // atem_packet_clear, atem_socket_recv, atem_socket_send
#include "./logs.h" // print_debug



// Sets new session id in handshake packet
void atem_handshake_newsessionid_set(uint8_t* packet, uint16_t sessionId) {
	atem_packet_word_set(packet, ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW, sessionId);
}

// Gets new session id from handshake packet
uint16_t atem_handshake_newsessionid_get(uint8_t* packet) {
	uint16_t sessionId = atem_packet_word_get(
		packet,
		ATEM_INDEX_NEWSESSIONID_HIGH,
		ATEM_INDEX_NEWSESSIONID_LOW
	);
	if (sessionId & 0x8000) {
		print_debug("Expected new session id 0x%04x to not have MSB set\n", sessionId);
		testrunner_abort();
	}
	return sessionId;
}

// Verifies new session id in handshake packet
void atem_handshake_newsessionid_verify(uint8_t* packet, uint16_t expectedNewSessionId) {
	uint16_t newSessionId = atem_handshake_newsessionid_get(packet);
	if (newSessionId == expectedNewSessionId) return;
	print_debug("Expected new session id 0x%04x, but got 0x%04x\n", expectedNewSessionId, newSessionId);
	testrunner_abort();
}



// Sets opcode in handshake packet, does not set session ids, retransmit flag or unknown id
void atem_handshake_opcode_set(uint8_t* packet, uint8_t opcode) {
	atem_header_flags_set(packet, ATEM_FLAG_SYN);
	atem_header_len_set(packet, ATEM_LEN_SYN);
	packet[ATEM_INDEX_OPCODE] = opcode;
}

// Gets opcode from handshake packet, does not check session ids, retransmit flag or unknown id
uint8_t atem_handshake_opcode_get(uint8_t* packet) {
	// Verifies packet is a SYN packet
	atem_header_flags_verify(packet, ATEM_FLAG_SYN, ATEM_FLAG_RETX);
	atem_header_len_verify(packet, ATEM_LEN_SYN);

	// Ack ID, local ID and remote ID should be clear for SYN packets
	atem_header_ackid_verify(packet, 0x0000);
	atem_header_localid_verify(packet, 0x0000);
	atem_header_remoteid_verify(packet, 0x0000);

	// Only sections for server assigned session ID and opcode can have data
	int expectClear[] = { 13, 16, 17, 18, 19 };
	for (int i = 0; i < (sizeof(expectClear) / sizeof(expectClear[0])); i++) {
		uint8_t byte = packet[expectClear[i]];
		if (byte == 0x00) continue;
		print_debug("Expected packet[%d] to be clear but it had the value 0x%02x\n", expectClear[i], packet[expectClear[i]]);
		testrunner_abort();
	}

	// Server assigned session ID should only be defined for successful responses
	uint8_t opcode = packet[ATEM_INDEX_OPCODE];
	if (opcode != ATEM_CONNECTION_SUCCESS) {
		atem_handshake_newsessionid_verify(packet, 0x0000);
	}
	else {
		uint16_t newSessionId = atem_handshake_newsessionid_get(packet);
		if (newSessionId & 0x8000) {
			print_debug("Expected new session id MSB to always be clear: 0x%04x\n", newSessionId);
			testrunner_abort();
		}
	}

	// Opening handshake uses client assigned session IDs while closing handshake uses server assigned
	uint16_t sessionId = atem_header_sessionid_get(packet);
	switch (opcode) {
		case ATEM_CONNECTION_OPEN:
		case ATEM_CONNECTION_SUCCESS:
		case ATEM_CONNECTION_REJECTED: {
			if (!(sessionId & 0x8000)) break;
			print_debug("Expected session id MSB to be set: 0x%04x\n", sessionId);
			testrunner_abort();
		}
		case ATEM_CONNECTION_CLOSING:
		case ATEM_CONNECTION_CLOSED: {
			if (sessionId & 0x8000) break;
			print_debug("Expected session id MSB to be set: 0x%4x\n", sessionId);
			testrunner_abort();
		}
		default: {
			print_debug("Unexpected opcode: %02x\n", opcode);
			testrunner_abort();
		}
	}

	return opcode;
}

// Verifies opcode in handshake packet
void atem_handshake_opcode_verify(uint8_t* packet, uint8_t expectedOpcode) {
	uint8_t opcode = atem_handshake_opcode_get(packet);
	if (opcode == expectedOpcode) return;
	print_debug("Expected opcode 0x%02x, but got 0x%02x\n", expectedOpcode, opcode);
	testrunner_abort();
}



// Sets opcode, retransmit flags and session id for handshake packet
void atem_handshake_set(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId) {
	atem_handshake_opcode_set(packet, opcode);
	atem_header_flags_set(packet, ATEM_FLAG_RETX * retx);
	atem_header_sessionid_set(packet, sessionId);
}

// Gets opcode from verified handshake packet
uint8_t atem_handshake_get(uint8_t* packet, bool retx, uint16_t sessionId) {
	uint8_t opcode = atem_handshake_opcode_get(packet);
	atem_header_flags_verify(packet, ATEM_FLAG_RETX * retx, ~ATEM_FLAG_RETX);
	atem_header_sessionid_verify(packet, sessionId);
	return opcode;
}

// Verifies opcode, retransmit flag and session id for handshake packet
void atem_handshake_verify(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId) {
	atem_handshake_opcode_verify(packet, opcode);
	atem_header_flags_verify(packet, ATEM_FLAG_RETX * retx, ~ATEM_FLAG_RETX);
	atem_header_sessionid_verify(packet, sessionId);
}



// Sends a handshake packet
void atem_handshake_send(int sock, uint8_t opcode, bool retx, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_packet_clear(packet);
	atem_handshake_set(packet, opcode, retx, sessionId);
	atem_socket_send(sock, packet);
}

// Receives a handshake packet with specified opcode and sessionId
void atem_handshake_recv(int sock, uint8_t opcode, bool retx, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	atem_handshake_verify(packet, opcode, retx, sessionId);
}
