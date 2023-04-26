#include <stdint.h> // uint8_t, uint16_t

#include "../../src/atem.h" // ATEM_MAX_PACKET_LEN
#include "../../src/atem_private.h" // ATEM_FLAG_ACK, ATEM_LEN_ACK
#include "./header.h" // atem_header_len_get_verify, atem_header_flags_set, atem_header_len_set, atem_header_sessionid_set, atem_header_ackid_set, atem_header_flags_get_verify, atem_header_sessionid_get_verify, atem_header_ackid_get_verify, atem_header_ackid_get
#include "./socket.h" // atem_socket_send, atem_socket_recv

// Sets packet to be an ACK packet
void atem_ack_set(uint8_t* packet, uint16_t sessionId, uint16_t ackId) {
	atem_header_flags_set(packet, ATEM_FLAG_ACK);
	atem_header_len_set(packet, ATEM_LEN_ACK);
	atem_header_sessionid_set(packet, sessionId);
	atem_header_ackid_set(packet, ackId);
}

// Gets ack id from ACK packet
uint16_t atem_ack_get(uint8_t* packet, uint16_t sessionId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACK, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	return atem_header_ackid_get(packet);
}

// Verifies ack id from ACK packet
void atem_ack_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t ackId) {
	atem_header_flags_get_verify(packet, ATEM_FLAG_ACK, 0);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, sessionId);
	atem_header_ackid_get_verify(packet, ackId);
}

// Sends an ACK packet
void atem_ack_send(int sock, uint16_t sessionId, uint16_t ackId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN] = {0};
	atem_ack_set(packet, sessionId, ackId);
	atem_socket_send(sock, packet);
}

// Gets ack id from verified ACK packet
uint16_t atem_ack_recv(int sock, uint16_t sessionId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	return atem_ack_get(packet, sessionId);
}

// Verifies ack id from verified ACK packet
void atem_ack_recv_verify(int sock, uint16_t sessionId, uint16_t ackId) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	atem_socket_recv(sock, packet);
	atem_ack_get_verify(packet, sessionId, ackId);
}
