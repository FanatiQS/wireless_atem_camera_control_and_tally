#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t
#include <assert.h> // assert
#include <string.h> // memcpy

#include "./atem_header.h" // atem_packet_word_set, atem_header_len_set, atem_header_len_get
#include "./atem_acknowledge.h" // atem_acknowledge_request_set
#include "./atem_sock.h" // atem_socket_send
#include "../../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "../../core/atem_protocol.h" // ATEM_LEN_HEADER, ATEM_LEN_CMDHEADER
#include "./atem_command.h"

// Appends a command buffer to a packet
void atem_command_append(uint8_t* packet, char* cmd_name, void* payload, uint16_t len) {
	assert(packet != NULL);
	assert(cmd_name != NULL);
	assert(strlen(cmd_name) == 4);
	assert(payload != NULL);
	assert(len > 0);
	assert(len < ATEM_PACKET_LEN_MAX);

	// Increments packet length
	uint16_t offset = atem_header_len_get(packet);
	atem_header_len_set(packet, offset + ATEM_LEN_CMDHEADER + len);

	// Appends command header data
	atem_packet_word_set(packet + offset, 0, 1, ATEM_LEN_CMDHEADER + len);
	memcpy(packet + offset + 4, cmd_name, 4);

	// Appends command payload data
	memcpy(packet + offset + ATEM_LEN_CMDHEADER, payload, len);
}

// Sends a command buffer
void atem_command_send(
	int sock,
	uint16_t session_id, uint16_t remote_id,
	char* cmd_name, void* payload, uint16_t len
) {
	uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
	atem_acknowledge_request_set(packet, session_id, remote_id);
	atem_command_append(packet, cmd_name, payload, len);
	atem_socket_send(sock, packet);
}

// Enqueues command into packet and sends when packet is full to start enqueueing on the next packet
void atem_command_queue(int sock, uint8_t* packet, char* cmd_name, void* payload, uint16_t len) {
	// Sends current packet if command can not fit
	assert(len < ATEM_PACKET_LEN_MAX);
	if (atem_header_len_get(packet) + len > ATEM_PACKET_LEN_MAX) {
		atem_socket_send(sock, packet);
		atem_header_remoteid_set(packet, atem_header_remoteid_get(packet) + 1);
		atem_header_len_set(packet, ATEM_LEN_HEADER);
	}

	// Buffers command to be transmitted when packet is full
	atem_command_append(packet, cmd_name, payload, len);
}
