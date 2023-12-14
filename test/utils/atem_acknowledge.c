#include <stdint.h> // uint8_t, uint16_t
#include <assert.h> // assert

#include "../../core/atem.h" // ATEM_MAX_PACKET_LEN
#include "../../core/atem_protocol.h" // ATEM_FLAG_ACK, ATEM_FLAG_ACKREQ, ATEM_LEN_ACK
#include "./atem_header.h" // atem_header_flags_set, atem_header_len_set, atem_header_sessionid_set, atem_header_remoteid_set, atem_header_flags_get_verify, atem_header_sessionid_get_verify, atem_header_localid_get_verify, atem_header_ackid_get_verify, atem_header_remoteid_get, atem_header_flags_remoteid_get_verify, atem_header_len_get_verify, atem_header_ackid_set, atem_header_ackid_get, atem_packet_clear
#include "./atem_sock.h" // atem_socket_send, atem_socket_recv
#include "./atem_acknowledge.h"



// Sets ackreq flag, length, session id and remote id for acknowledgement request
void atem_acknowledge_request_set(uint8_t* packet, uint16_t sessionId, uint16_t remoteId) {
	atem_header_flags_set(packet, ATEM_FLAG_ACKREQ);
	atem_header_len_set(packet, ATEM_LEN_ACK);
	atem_header_sessionid_set(packet, sessionId);
	atem_header_remoteid_set(packet, remoteId);
}

// Gets remote id from verified acknowledge request packet, does not check unknown id
uint16_t atem_acknowledge_request_get(uint8_t* packet, uint16_t sessionId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_ackid_get_verify(packet, 0x0000);
	return atem_header_remoteid_get(packet);
}

// Verifies remote id in verified acknowledge request packet, does not check unknown id
void atem_acknowledge_request_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t remoteId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACKREQ, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_ackid_get_verify(packet, 0x0000);
	atem_header_remoteid_get_verify(packet, remoteId);
}

// Sends acknowledge request packet with speficied remote id and session id
void atem_acknowledge_request_send(int sock, uint16_t sessionId, uint16_t remoteId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN] = {0};
	atem_acknowledge_request_set(packet, sessionId, remoteId);
	atem_socket_send(sock, packet);
}

// Gets remote id from received verified acknowledge request packet
uint16_t atem_acknowledge_request_recv(int sock, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	return atem_acknowledge_request_get(packet, sessionId);
}

// Verifies remote id in verified acknowledge request packet
void atem_acknowledge_request_recv_verify(int sock, uint16_t sessionId, uint16_t remoteId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	atem_acknowledge_request_get_verify(packet, sessionId, remoteId);
}



// Sets ack flag, length, session id and ack id for acknowledgement response
void atem_acknowledge_response_set(uint8_t* packet, uint16_t sessionId, uint16_t ackId) {
	atem_header_flags_set(packet, ATEM_FLAG_ACK);
	atem_header_len_set(packet, ATEM_LEN_ACK);
	atem_header_sessionid_set(packet, sessionId);
	atem_header_ackid_set(packet, ackId);
}

// Gets ack id from verified acknowledge packet, does not check unknown id
uint16_t atem_acknowledge_response_get(uint8_t* packet, uint16_t sessionId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACK, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_remoteid_get_verify(packet, 0x0000);
	return atem_header_ackid_get(packet);
}

// Verifies ack id in verified acknowledge packet, does not check unknown id
void atem_acknowledge_response_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t ackId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACK, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_ackid_get_verify(packet, ackId);
	atem_header_localid_get_verify(packet, 0x0000);
	atem_header_remoteid_get_verify(packet, 0x0000);
}

// Sends acknowledge packet with speficied ack id and session id
void atem_acknowledge_response_send(int sock, uint16_t sessionId, uint16_t ackId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN] = {0};
	atem_acknowledge_response_set(packet, sessionId, ackId);
	atem_socket_send(sock, packet);
}

// Gets ack id from received verified acknowledge packet
uint16_t atem_acknowledge_response_recv(int sock, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	return atem_acknowledge_response_get(packet, sessionId);
}

// Verifies ack id in verified acknowledge packet
void atem_acknowledge_response_recv_verify(int sock, uint16_t sessionId, uint16_t ackId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	atem_acknowledge_response_get_verify(packet, sessionId, ackId);
}



// Receives data and responds to pings
bool atem_keepalive(int sock, uint8_t* packet, uint16_t sessionId) {
	atem_socket_recv(sock, packet);
	if (!(atem_header_flags_get(packet) & ATEM_FLAG_ACKREQ)) return false;
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_acknowledge_request_send(sock, sessionId, atem_header_remoteid_get(packet));
	return true;
}



// Tests functions in this file
void atem_acknowledge_init(void) {
	uint8_t packet[ATEM_MAX_PACKET_LEN] = {0};

	// Tests getter/setter for acknowledgement request
	atem_packet_clear(packet);
	atem_acknowledge_request_set(packet, 0x1111, 0x2222);
	assert(atem_acknowledge_request_get(packet, 0x1111) == 0x2222);

	// Tests getter/setter for acknowledgement response
	atem_packet_clear(packet);
	atem_acknowledge_response_set(packet, 0x3333, 0x4444);
	assert(atem_acknowledge_response_get(packet, 0x3333) == 0x4444);
}
