#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool, false
#include <stdio.h> // fprintf, stderr, printf
#include <stdlib.h> // abort
#include <assert.h> // assert

#include "../../core/atem_protocol.h" // ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW, ATEM_FLAG_SYN, ATEM_LEN_SYN, ATEM_INDEX_OPCODE, ATEM_FLAG_RETX, ATEM_OPCODE_OPEN, ATEM_OPCODE_ACCEPT, ATEM_OPCODE_REJECT, ATEM_OPCODE_CLOSING, ATEM_OPCODE_CLOSED
#include "../../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "./atem_header.h" // atem_packet_clear, atem_packet_word_set, atem_packet_word_get, atem_header_flags_set, atem_header_flags_get_verify, atem_header_len_set, atem_header_len_get_verify, atem_header_sessionid_get, atem_header_ackid_get_verify, atem_header_localid_get_verify, atem_header_remoteid_get_verify, atem_header_sessionid_set, atem_header_sessionid_get_verify, atem_header_flags_isnotset
#include "./atem_acknowledge.h" // atem_acknowledge_response_send, atem_acknowledge_response_recv_verify, atem_acknowledge_keepalive
#include "./atem_sock.h" // atem_socket_recv, atem_socket_send, atem_socket_connect, atem_socket_listen, atem_socket_create, atem_socket_close
#include "./logs.h" // logs_find
#include "./atem_handshake.h"



// Sets new session id in handshake packet
void atem_handshake_newsessionid_set(uint8_t* packet, uint16_t session_id_new) {
	atem_packet_word_set(packet, ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW, session_id_new);
}

// Gets new session id from handshake packet
uint16_t atem_handshake_newsessionid_get(uint8_t* packet) {
	uint16_t session_id = atem_packet_word_get(packet, ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW);
	if (session_id & 0x8000) {
		fprintf(stderr, "Expected new session id 0x%04x to not have MSB set\n", session_id);
		abort();
	}
	return session_id;
}

// Ensures new session id in handshake packet matches expected new session id
void atem_handshake_newsessionid_get_verify(uint8_t* packet, uint16_t session_id_new_expected) {
	uint16_t session_id_new = atem_handshake_newsessionid_get(packet);
	if (session_id_new == session_id_new_expected) return;
	fprintf(stderr, "Expected new session id 0x%04x, but got 0x%04x\n", session_id_new_expected, session_id_new);
	abort();
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
	atem_header_flags_get_verify(packet, ATEM_FLAG_SYN, ATEM_FLAG_RETX);
	atem_header_len_get_verify(packet, ATEM_LEN_SYN);

	// Ack ID, local ID and remote ID should be clear for SYN packets
	atem_header_ackid_get_verify(packet, 0x0000);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_remoteid_get_verify(packet, 0x0000);

	// Only sections for server assigned session ID and opcode can have data
	int expect_clear[] = { 13, 16, 17, 18, 19 };
	for (size_t i = 0; i < (sizeof(expect_clear) / sizeof(expect_clear[0])); i++) {
		uint8_t byte = packet[expect_clear[i]];
		if (byte == 0x00) continue;
		fprintf(
			stderr,
			"Expected packet[%d] to be clear but it had the value 0x%02x\n",
			expect_clear[i], packet[expect_clear[i]]
		);
		abort();
	}

	uint8_t opcode = packet[ATEM_INDEX_OPCODE];

	// Server assigned session id should not have its MSB set when received in SYN/ACK
	if (opcode == ATEM_OPCODE_ACCEPT) {
		atem_handshake_newsessionid_get(packet);
	}
	// Server assigned session id should only be defined for successful opening handshake responses
	else {
		atem_handshake_newsessionid_get_verify(packet, 0x0000);
	}

	// Opening handshake uses client assigned session IDs while closing handshake uses server assigned
	uint16_t session_id = atem_header_sessionid_get(packet);
	switch (opcode) {
		case ATEM_OPCODE_OPEN:
		case ATEM_OPCODE_ACCEPT:
		case ATEM_OPCODE_REJECT: {
			if (!(session_id & 0x8000)) break;
			fprintf(stderr, "Expected session id MSB to be clear: 0x%04x\n", session_id);
			abort();
		}
		case ATEM_OPCODE_CLOSING:
		case ATEM_OPCODE_CLOSED: {
			if (session_id & 0x8000) break;
			fprintf(stderr, "Expected session id MSB to be set: 0x%04x\n", session_id);
			abort();
		}
		default: {
			fprintf(stderr, "Unexpected opcode: %02x\n", opcode);
			abort();
		}
	}

	return opcode;
}

// Ensures opcode in handshake packet matches expected opcode
void atem_handshake_opcode_get_verify(uint8_t* packet, uint8_t opcode_expected) {
	uint8_t opcode = atem_handshake_opcode_get(packet);
	if (opcode == opcode_expected) return;
	fprintf(stderr, "Expected opcode 0x%02x, but got 0x%02x\n", opcode_expected, opcode);
	abort();
}



// Sets ATEM packet specifying opcode, retransmit flag and session id for handshake packet
void atem_handshake_sessionid_set(uint8_t* packet, uint8_t opcode, bool retx, uint16_t session_id) {
	atem_handshake_opcode_set(packet, opcode);
	atem_header_flags_set(packet, ATEM_FLAG_RETX * retx);
	atem_header_sessionid_set(packet, session_id);
}

// Gets session id from verified handshake packet
uint16_t atem_handshake_sessionid_get(uint8_t* packet, uint8_t opcode, bool retx) {
	atem_handshake_opcode_get_verify(packet, opcode);
	atem_header_flags_isset(packet, ATEM_FLAG_RETX * retx);
	atem_header_flags_isnotset(packet, ATEM_FLAG_RETX * !retx);
	return atem_header_sessionid_get(packet);
}

// Ensures opcode, retransmit flag and session id for handshake packet matches expected values
void atem_handshake_sessionid_get_verify(uint8_t* packet, uint8_t opcode, bool retx, uint16_t session_id) {
	atem_handshake_opcode_get_verify(packet, opcode);
	atem_header_flags_isset(packet, ATEM_FLAG_RETX * retx);
	atem_header_flags_isnotset(packet, ATEM_FLAG_RETX * !retx);
	atem_header_sessionid_get_verify(packet, session_id);
}

// Sends handshake packet with opcode, retransmit flag and session id
void atem_handshake_sessionid_send(int sock, uint8_t opcode, bool retx, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
	atem_handshake_sessionid_set(packet, opcode, retx, session_id);
	atem_socket_send(sock, packet);
}

// Gets session id from handshake packet and verifies opcode and retransmit flag
uint16_t atem_handshake_sessionid_recv(int sock, uint8_t opcode, bool retx) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	return atem_handshake_sessionid_get(packet, opcode, retx);
}

// Ensures opcode, retransmit flag and session id in received handshake packet matches expected values
void atem_handshake_sessionid_recv_verify(int sock, uint8_t opcode, bool retx, uint16_t session_id_expected) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_handshake_sessionid_get_verify(packet, opcode, retx, session_id_expected);
}



// Sends handshake packet with opcode, retransmit flag, session id and new session id
void atem_handshake_newsessionid_send(int sock, uint8_t opcode, bool retx, uint16_t session_id, uint16_t session_id_new) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
	atem_handshake_sessionid_set(packet, opcode, retx, session_id);
	atem_handshake_newsessionid_set(packet, session_id_new);
	atem_socket_send(sock, packet);
}

// Gets new session id from received handshake packet and verifies opcode, retransmit flag and session id
uint16_t atem_handshake_newsessionid_recv(int sock, uint8_t opcode, bool retx, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_handshake_sessionid_get_verify(packet, opcode, retx, session_id);
	return atem_handshake_newsessionid_get(packet);
}

// Ensures opcode, retransmit flag, session id and new session id in received handshake packet matches expected values
void atem_handshake_newsessionid_recv_verify(int sock, uint8_t opcode, bool retx, uint16_t session_id, uint16_t session_id_new_expected) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_handshake_sessionid_get_verify(packet, opcode, retx, session_id);
	atem_handshake_newsessionid_get_verify(packet, session_id_new_expected);
}



// Forces the ATEM client trying to connect to restart the connection
void atem_handshake_resetpeer(void) {
	bool logs_enabled = logs_find("atem_recv") || logs_find("atem_send");
	if (logs_enabled) {
		printf("Resetting client handshake\n");
	}

	uint8_t packet[ATEM_PACKET_LEN_MAX];
	int sock = atem_socket_create();
	atem_socket_listen(sock, packet);
	atem_handshake_opcode_get_verify(packet, ATEM_OPCODE_OPEN);
	uint16_t session_id = atem_header_sessionid_get(packet);
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, session_id);
	atem_socket_close(sock);

	if (logs_enabled) {
		printf("Client handshake reset\n");
	}
}



// Sends opening handshake request, receives response and returns server assigned session id without MSB set (session_id_new)
uint16_t atem_handshake_start_client(int sock, uint16_t session_id) {
	atem_socket_connect(sock);
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, session_id);
	return atem_handshake_newsessionid_recv(sock, ATEM_OPCODE_ACCEPT, false, session_id);
}

// Receives an opening handshake request from an ATEM client and returns client assigned session id
uint16_t atem_handshake_start_server(int sock) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_listen(sock, packet);
	return atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);
}



// Connects to the ATEM switcher by completing entire opening handshake
uint16_t atem_handshake_connect(int sock, uint16_t session_id) {
	uint16_t session_id_new = atem_handshake_start_client(sock, session_id);
	atem_acknowledge_response_send(sock, session_id, 0x0000);
	return session_id_new | 0x8000;
}

// Connects to the ATEM switcher by completing entire opening handshake allowing rejection by server
uint16_t atem_handshake_tryconnect(int sock, uint16_t session_id) {
	atem_socket_connect(sock);
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_OPEN, false, session_id);

	// Reads ATEM packets until receiving non acknowledge requests
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	while (atem_acknowledge_keepalive(sock, packet));
	atem_header_sessionid_get_verify(packet, session_id);
	atem_header_flags_isnotset(packet, ATEM_FLAG_RETX);

	// Returns falsy on rejected since all valid server assigned session ids have MSB set
	if (atem_handshake_opcode_get(packet) == ATEM_OPCODE_REJECT) {
		return 0x0000;
	}

	// Completes opening handshake and returns server assigned session id
	atem_handshake_opcode_get_verify(packet, ATEM_OPCODE_ACCEPT);
	uint16_t session_id_new = atem_handshake_newsessionid_get(packet);
	atem_acknowledge_response_send(sock, session_id, 0x0000);
	return session_id_new | 0x8000;
}

// Gets ATEM client connection by completing entire opening handshake, does not enforce no retransmit flag in request
uint16_t atem_handshake_listen(int sock, uint16_t session_id_new) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_listen(sock, packet);
	atem_handshake_opcode_get_verify(packet, ATEM_OPCODE_OPEN);
	uint16_t session_id = atem_header_sessionid_get(packet);
	atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, session_id, session_id_new);
	atem_acknowledge_response_recv_verify(sock, session_id, 0x0000);
	return session_id_new | 0x8000;
}

// Closes connection to ATEM switcher or client by completing entire closing handshake
void atem_handshake_close(int sock, uint16_t session_id) {
	atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, session_id);
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	uint8_t flags;
	do {
		atem_socket_recv(sock, packet);
		flags = atem_header_flags_get(packet);
	} while (flags != ATEM_FLAG_SYN || atem_handshake_opcode_get(packet) != ATEM_OPCODE_CLOSED);
	atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSED, false, session_id);
}



// Connects sessions until ATEM server is full and returns how many could connect
uint16_t atem_handshake_fill(int sock) {
	bool logs_enabled = logs_find("atem_recv") || logs_find("atem_send");
	if (logs_enabled) {
		printf("Filling up server\n");
	}

	uint16_t count = 0;
	while (atem_handshake_tryconnect(sock, count)) {
		count++;
	}

	if (logs_enabled) {
		printf("Server filled\n");
	}

	return count;
}

// Flushes all data received from ATEM server after opening handshake up to first ping
void atem_handshake_flush(int sock, uint16_t session_id) {
	uint16_t remote_id = 0x0001;
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	do {
		atem_acknowledge_keepalive(sock, packet);
		atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK | ATEM_FLAG_RETX);
		atem_header_sessionid_get_verify(packet, session_id);
		atem_header_ackid_get_verify(packet, 0x0000);
		atem_header_localid_get_verify(packet, 0x0000);
		if (remote_id >= atem_header_remoteid_get(packet)) {
			atem_header_remoteid_get_verify(packet, remote_id);
			remote_id++;
		}
	} while (atem_header_len_get(packet) > ATEM_LEN_HEADER);
}



// Tests functions in this file
void atem_handshake_init(void) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];

	// Tests getter/setter for new session id
	atem_packet_clear(packet);
	atem_handshake_newsessionid_set(packet, 0x7fff);
	assert(atem_handshake_newsessionid_get(packet) == 0x7fff);

	// Tests getter/setter for opcode
	atem_packet_clear(packet);
	atem_handshake_opcode_set(packet, ATEM_OPCODE_OPEN);
	assert(atem_handshake_opcode_get(packet) == ATEM_OPCODE_OPEN);
}
