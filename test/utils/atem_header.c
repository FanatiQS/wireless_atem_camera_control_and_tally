#include <stdint.h> // uint8_t, uint16_t
#include <string.h> // memset
#include <assert.h> // assert
#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stddef.h> // size_t
#include <stdbool.h> // bool

#include "../../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "../../core/atem_protocol.h" // ATEM_MASK_LEN_HIGH, ATEM_INDEX_FLAGS, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW
#include "./atem_header.h"



// Zeroes out packet memory
void atem_packet_clear(uint8_t* packet) {
	memset(packet, 0, ATEM_PACKET_LEN_MAX);
}



// Sets 16bit word in packet
void atem_packet_word_set(uint8_t* packet, uint8_t high, uint8_t low, uint16_t word) {
	assert(high + 1 == low);
	packet[high] = (uint8_t)(word >> 8);
	packet[low] = (uint8_t)(word & 0xff);
}

// Gets 16bit word from packet
uint16_t atem_packet_word_get(uint8_t* packet, uint8_t high, uint8_t low) {
	assert(high + 1 == low);
	return ((packet[high] << 8) | packet[low]) & 0xffff;
}



// Clears ATEM flags in packet
void atem_header_flags_clear(uint8_t* packet) {
	packet[ATEM_INDEX_FLAGS] &= ATEM_MASK_LEN_HIGH;
}

// Sets ATEM flags in packet while leaving any previously set flags intact
void atem_header_flags_set(uint8_t* packet, uint8_t flags) {
	packet[ATEM_INDEX_FLAGS] |= flags;
}

// Gets ATEM flags from packet
uint8_t atem_header_flags_get(uint8_t* packet) {
	return packet[ATEM_INDEX_FLAGS] & ~ATEM_MASK_LEN_HIGH;
}

// Ensures ATEM packet has all required flags set without any illegal flag
void atem_header_flags_get_verify(uint8_t* packet, uint8_t flags_required, uint8_t flags_optional) {
	uint8_t flags = atem_header_flags_get(packet);

	// Verifies required flags are all set
	if ((flags & flags_required) != flags_required) {
		fprintf(stderr, "Missing required flag(s) 0x%02x from 0x%02x\n", flags_required, flags);
		abort();
	}

	// Verifies only required or optional flags are set
	uint8_t illegalFlags = ~(flags_required | flags_optional);
	if (flags & illegalFlags) {
		fprintf(stderr, "Unexpected illegal flag(s) 0x%02x\n", illegalFlags & flags);
		abort();
	}
}



// Ensures specified ATEM flags are set
void atem_header_flags_isset(uint8_t* packet, uint8_t flags) {
	atem_header_flags_get_verify(packet, flags, ~flags);
}

// Ensures specified ATEM flags are clear
void atem_header_flags_isnotset(uint8_t* packet, uint8_t flags) {
	atem_header_flags_get_verify(packet, 0, ~flags);
}



// Sets length of packet
void atem_header_len_set(uint8_t* packet, uint16_t len) {
	len |= atem_header_flags_get(packet) << 8;
	atem_packet_word_set(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, len);
}

// Gets length of packet
uint16_t atem_header_len_get(uint8_t* packet) {
	uint16_t word = atem_packet_word_get(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW);
	uint16_t len = word & ATEM_PACKET_LEN_MAX;
	if (len < ATEM_LEN_HEADER) {
		fprintf(stderr, "Packet is smaller than minimum size: %d\n", len);
		abort();
	}
	return len;
}

// Ensures length of packet matches expected length
void atem_header_len_get_verify(uint8_t* packet, size_t len_expected) {
	uint16_t len = atem_header_len_get(packet);
	if (len == len_expected) return;
	fprintf(stderr, "Expected packet length %zu, but got %d\n", len_expected, len);
	abort();
}



/**
 * Gets the next session id to help give every request gets its own session id
 * @param msb Defines if session id is a server assigned session id or client assigned session id
 */
uint16_t atem_header_sessionid_next(bool msb) {
	static uint16_t session_id = 0;
	return (++session_id & 0x7fff) | ((msb << 15) & 0xffff);
}

// Sets session id in packet
void atem_header_sessionid_set(uint8_t* packet, uint16_t session_id) {
	atem_packet_word_set(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, session_id);
}

// Gets session id from packet
uint16_t atem_header_sessionid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW);
}

// Ensures session id in packet matches expected session id
void atem_header_sessionid_get_verify(uint8_t* packet, uint16_t session_id_expected) {
	uint16_t session_id = atem_header_sessionid_get(packet);
	if (session_id == session_id_expected) return;
	fprintf(stderr, "Expected session id 0x%04x, but got 0x%04x\n", session_id_expected, session_id);
	abort();
}



// Sets ack id in packet
void atem_header_ackid_set(uint8_t* packet, uint16_t ack_id) {
	atem_packet_word_set(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ack_id);
}

// Gets ack id from packet
uint16_t atem_header_ackid_get(uint8_t* packet) {
	uint16_t ack_id = atem_packet_word_get(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW);
	if (ack_id & 0x8000) {
		fprintf(stderr, "Expected ack id to never have MSB set: %04x\n", ack_id);
		abort();
	}
	return ack_id;
}

// Ensures ack id in packet matches expected ack id
void atem_header_ackid_get_verify(uint8_t* packet, uint16_t expectedAckId) {
	uint16_t ackId = atem_header_ackid_get(packet);
	if (ackId == expectedAckId) return;
	fprintf(stderr, "Expected ack id 0x%04x, but got 0x%04x\n", expectedAckId, ackId);
	abort();
}



// Sets local id in packet
void atem_header_localid_set(uint8_t* packet, uint16_t local_id) {
	atem_packet_word_set(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, local_id);
}

// Gets local id from packet
uint16_t atem_header_localid_get(uint8_t* packet) {
	uint16_t local_id = atem_packet_word_get(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW);
	if (local_id & 0x8000) {
		fprintf(stderr, "Expected local id to never have MSB set: %04x\n", local_id);
		abort();
	}
	return local_id;
}

// Ensures local id in packet matches expected local id
void atem_header_localid_get_verify(uint8_t* packet, uint16_t local_id_expected) {
	uint16_t local_id = atem_header_localid_get(packet);
	if (local_id == local_id_expected) return;
	fprintf(stderr, "Expected local id 0x%04x, but got 0x%04x\n", local_id_expected, local_id);
	abort();
}



// Sets unknown id in packet
void atem_header_unknownid_set(uint8_t* packet, uint16_t unknown_id) {
	atem_packet_word_set(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW, unknown_id);
}

// Gets unknown id from packet
uint16_t atem_header_unknownid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW);
}

// Ensures unknown id in packet matches expected unknown id
void atem_header_unknownid_get_verify(uint8_t* packet, uint16_t unknown_id_expected) {
	uint16_t unknown_id = atem_header_unknownid_get(packet);
	if (unknown_id == unknown_id_expected) return;
	fprintf(stderr, "Expected unknown id 0x%04x, but got 0x%04x\n", unknown_id_expected, unknown_id);
	abort();
}



// Sets remote id in packet
void atem_header_remoteid_set(uint8_t* packet, uint16_t remote_id) {
	atem_packet_word_set(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, remote_id);
}

// Gets remote id from packet
uint16_t atem_header_remoteid_get(uint8_t* packet) {
	uint16_t remote_id = atem_packet_word_get(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW);
	if (remote_id & 0x8000) {
		fprintf(stderr, "Expected remote id to never have MSB set: %04x\n", remote_id);
		abort();
	}
	return remote_id;
}

// Ensures remote id in packet matches expected unknown id
void atem_header_remoteid_get_verify(uint8_t* packet, uint16_t remote_id_expected) {
	uint16_t remote_id = atem_header_remoteid_get(packet);
	if (remote_id == remote_id_expected) return;
	fprintf(stderr, "Expected remote id 0x%04x, but got 0x%04x\n", remote_id_expected, remote_id);
	abort();
}



// Tests functions in this file
void atem_header_init(void) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	const uint8_t flags_all = 0xff & ~(ATEM_PACKET_LEN_MAX >> 8);

	// Tests atem_packet_word_set and atem_packet_word_get
	atem_packet_clear(packet);
	assert(atem_packet_word_get(packet, 0, 1) == 0x0000);
	atem_packet_word_set(packet, 0, 1, 0xffff);
	assert(atem_packet_word_get(packet, 0, 1) == 0xffff);
	assert(atem_packet_word_get(packet, 1, 2) == 0xff00);
	assert(atem_packet_word_get(packet, 2, 3) == 0x0000);
	atem_packet_word_set(packet, 2, 3, 0x0f01);
	assert(atem_packet_word_get(packet, 2, 3) == 0x0f01);

	// Tests atem_header_flags_get and atem_header_flags_set
	atem_packet_clear(packet);
	assert(atem_header_flags_get(packet) == 0);
	atem_header_flags_set(packet, ATEM_FLAG_SYN);
	assert(atem_header_flags_get(packet) == ATEM_FLAG_SYN);
	atem_header_flags_set(packet, ATEM_FLAG_RETX);
	assert(atem_header_flags_get(packet) == (ATEM_FLAG_SYN | ATEM_FLAG_RETX));
	atem_header_flags_set(packet, flags_all);
	assert(atem_header_flags_get(packet) == flags_all);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_flags_get(packet) == flags_all);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);

	// Tests atem_header_flags_get_verify
	atem_packet_clear(packet);
	atem_header_flags_get_verify(packet, 0, 0);
	atem_header_flags_set(packet, flags_all);
	atem_header_flags_get_verify(packet, flags_all, 0);
	atem_header_flags_get_verify(packet, 0, flags_all);

	// Tests atem_header_flags_clear
	atem_packet_clear(packet);
	assert(atem_header_flags_get(packet) == 0);
	atem_header_flags_set(packet, flags_all);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_flags_get(packet) == flags_all);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);
	atem_header_flags_clear(packet);
	assert(atem_header_flags_get(packet) == 0x00);

	// Tests atem_header_flags_get with max length
	atem_packet_clear(packet);
	assert(atem_packet_word_get(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW) == 0);
	atem_header_len_set(packet, ATEM_PACKET_LEN_MAX);
	assert(atem_header_len_get(packet) == ATEM_PACKET_LEN_MAX);
	assert(atem_header_flags_get(packet) == 0);

	// Tests atem_header_len_get and atem_header_len_set
	atem_packet_clear(packet);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);
	atem_header_len_set(packet, 100);
	assert(atem_header_len_get(packet) == 100);

	// Tests atem_header_sessionid_next
	for (uint16_t i = 1; i < 0x8000; i++) {
		assert(atem_header_sessionid_next(false) == i);
	}
	assert(atem_header_sessionid_next(false) == 0x0000);
	for (uint16_t i = 1; i < 0x8000; i++) {
		assert(atem_header_sessionid_next(true) == (i | 0x8000));
	}
	assert(atem_header_sessionid_next(true) == 0x8000);
	assert(atem_header_sessionid_next(true) == 0x8001);


	// Tests atem_header_sessionid_get and atem_header_sessionid_set
	atem_packet_clear(packet);
	assert(atem_header_sessionid_get(packet) == 0);
	atem_header_sessionid_set(packet, 0x1eee);
	assert(atem_header_sessionid_get(packet) == 0x1eee);

	// Tests atem_header_ackid_get and atem_header_ackid_set
	atem_packet_clear(packet);
	assert(atem_header_ackid_get(packet) == 0);
	atem_header_ackid_set(packet, 0x1eee);
	assert(atem_header_ackid_get(packet) == 0x1eee);

	// Tests atem_header_localid_get and atem_header_localid_set
	atem_packet_clear(packet);
	assert(atem_header_localid_get(packet) == 0);
	atem_header_localid_set(packet, 0x1eee);
	assert(atem_header_localid_get(packet) == 0x1eee);

	// Tests atem_header_remoteid_get and atem_header_remoteid_set
	atem_packet_clear(packet);
	assert(atem_header_remoteid_get(packet) == 0);
	atem_header_remoteid_set(packet, 0x1eee);
	assert(atem_header_remoteid_get(packet) == 0x1eee);
}
