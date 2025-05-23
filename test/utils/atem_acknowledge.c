#include <stdint.h> // uint8_t, uint16_t
#include <assert.h> // assert
#include <stdbool.h> // bool, true, false
#include <stddef.h> // NULL
#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort

#include "../../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "../../core/atem_protocol.h" // ATEM_FLAG_ACK, ATEM_FLAG_ACKREQ, ATEM_LEN_ACK
#include "./atem_header.h" // atem_header_flags_set, atem_header_len_set, atem_header_sessionid_set, atem_header_remoteid_set, atem_header_flags_get_verify, atem_header_sessionid_get_verify, atem_header_localid_get_verify, atem_header_ackid_get_verify, atem_header_remoteid_get, atem_header_flags_remoteid_get_verify, atem_header_len_get_verify, atem_header_ackid_set, atem_header_ackid_get, atem_packet_clear, atem_header_flags_get, ATEM_FLAG_RETX, atem_header_sessionid_get
#include "./atem_sock.h" // atem_socket_send, atem_socket_recv
#include "./timediff.h" // timediff_mark, timediff_get
#include "./atem_acknowledge.h"



// Sets ackreq flag, length, session id and remote id for acknowledgement request
void atem_acknowledge_request_set(uint8_t* packet, uint16_t session_id, uint16_t remote_id) {
	atem_header_flags_clear(packet);
	atem_header_flags_set(packet, ATEM_FLAG_ACKREQ);
	atem_header_len_set(packet, ATEM_LEN_ACK);
	atem_header_sessionid_set(packet, session_id);
	atem_header_remoteid_set(packet, remote_id);
}

// Gets remote id from verified acknowledge request packet, does not check unknown id
uint16_t atem_acknowledge_request_get(uint8_t* packet, uint16_t session_id) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, session_id);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_ackid_get_verify(packet, 0x0000);
	return atem_header_remoteid_get(packet);
}

// Ensures remote id in verified acknowledge request packet matches expected remote id, does not check unknown id
void atem_acknowledge_request_get_verify(uint8_t* packet, uint16_t session_id, uint16_t remote_id) {
	atem_acknowledge_request_get(packet, session_id);
	atem_header_remoteid_get_verify(packet, remote_id);
}

// Sends acknowledge request packet with specified remote id and session id
void atem_acknowledge_request_send(int sock, uint16_t session_id, uint16_t remote_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
	atem_acknowledge_request_set(packet, session_id, remote_id);
	atem_socket_send(sock, packet);
}

// Gets remote id from received verified acknowledge request packet
uint16_t atem_acknowledge_request_recv(int sock, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	return atem_acknowledge_request_get(packet, session_id);
}

// Ensures remote id in verified acknowledge request packet matches expected remote id
void atem_acknowledge_request_recv_verify(int sock, uint16_t session_id, uint16_t remote_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_acknowledge_request_get_verify(packet, session_id, remote_id);
}



// Sets ack flag, length, session id and ack id for acknowledgement response
void atem_acknowledge_response_set(uint8_t* packet, uint16_t session_id, uint16_t ack_id) {
	atem_header_flags_clear(packet);
	atem_header_flags_set(packet, ATEM_FLAG_ACK);
	atem_header_len_set(packet, ATEM_LEN_ACK);
	atem_header_sessionid_set(packet, session_id);
	atem_header_ackid_set(packet, ack_id);
}

// Gets ack id from verified acknowledge packet, does not check unknown id
uint16_t atem_acknowledge_response_get(uint8_t* packet, uint16_t session_id) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACK, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, session_id);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_remoteid_get_verify(packet, 0x0000);
	return atem_header_ackid_get(packet);
}

// Ensures ack id in verified acknowledge packet matches expected ack id, does not check unknown id
void atem_acknowledge_response_get_verify(uint8_t* packet, uint16_t session_id, uint16_t ack_id) {
	atem_acknowledge_response_get(packet, session_id);
	atem_header_ackid_get_verify(packet, ack_id);
}

// Sends acknowledge packet with specified ack id and session id
void atem_acknowledge_response_send(int sock, uint16_t session_id, uint16_t ack_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
	atem_acknowledge_response_set(packet, session_id, ack_id);
	atem_socket_send(sock, packet);
}

// Gets ack id from received verified acknowledge packet
uint16_t atem_acknowledge_response_recv(int sock, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	return atem_acknowledge_response_get(packet, session_id);
}

// Ensures ack id in verified acknowledge packet matches expected ack id
void atem_acknowledge_response_recv_verify(int sock, uint16_t session_id, uint16_t ack_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_acknowledge_response_get_verify(packet, session_id, ack_id);
}



// Receives data and automatically acknowledges if required
bool atem_acknowledge_keepalive(int sock, uint8_t* packet) {
	uint8_t _packet[ATEM_PACKET_LEN_MAX];
	if (packet == NULL) {
		packet = _packet;
	}
	atem_socket_recv(sock, packet);
	if (!(atem_header_flags_get(packet) & ATEM_FLAG_ACKREQ)) {
		return false;
	}
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, ATEM_FLAG_ACK | ATEM_FLAG_RETX);
	uint16_t session_id = atem_header_sessionid_get(packet);
	uint16_t remote_id = atem_header_remoteid_get(packet);
	atem_acknowledge_response_send(sock, session_id, remote_id);
	return true;
}

// Flushes all data until next acknowledgement request
uint16_t atem_acknowledge_request_flush(int sock, uint16_t session_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	while (!atem_acknowledge_keepalive(sock, packet));
	atem_header_sessionid_get_verify(packet, session_id);
	return atem_header_remoteid_get(packet);
}

// Flushes all data until acknowledgement for specified remote id is received
void atem_acknowledge_response_flush(int sock, uint16_t session_id, uint16_t remote_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	struct timespec mark = timediff_mark();
	do {
		if (timediff_get(mark) >= 2000) {
			fprintf(stderr, "Did not get acknowledgement for max size packet\n");
			abort();
		}
		atem_acknowledge_keepalive(sock, packet);
		atem_header_sessionid_get_verify(packet, session_id);
	} while (!(atem_header_flags_get(packet) & ATEM_FLAG_ACK) || atem_header_ackid_get(packet) < remote_id);
	atem_header_ackid_get_verify(packet, remote_id);
}



// Tests functions in this file
void atem_acknowledge_init(void) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};

	// Tests getter/setter for acknowledgement request
	atem_packet_clear(packet);
	atem_acknowledge_request_set(packet, 0x1111, 0x2222);
	assert(atem_acknowledge_request_get(packet, 0x1111) == 0x2222);

	// Tests getter/setter for acknowledgement response
	atem_packet_clear(packet);
	atem_acknowledge_response_set(packet, 0x3333, 0x4444);
	assert(atem_acknowledge_response_get(packet, 0x3333) == 0x4444);
}
