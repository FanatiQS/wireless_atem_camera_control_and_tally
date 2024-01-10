#include <stdint.h> // uint8_t, uint16_t
#include <string.h> // memset
#include <assert.h> // assert
#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stddef.h> // size_t

#include "../../core/atem.h" // ATEM_MAX_PACKET_LEN
#include "../../core/atem_protocol.h" // ATEM_MASK_LEN_HIGH, ATEM_INDEX_FLAGS, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW
#include "./atem_header.h"



// Zeroes out packet memory
void atem_packet_clear(uint8_t* packet) {
	memset(packet, 0, ATEM_MAX_PACKET_LEN);
}



// Sets 16bit word in packet
void atem_packet_word_set(uint8_t* packet, uint8_t high, uint8_t low, uint16_t word) {
	packet[high] = (uint8_t)(word >> 8);
	packet[low] = (uint8_t)(word & 0xff);
}

// Gets 16bit word from packet
uint16_t atem_packet_word_get(uint8_t* packet, uint8_t high, uint8_t low) {
	assert(high + 1 == low);
	return (uint16_t)((packet[high] << 8) | packet[low]);
}



// Clears ATEM flags in packet
void atem_header_flags_clear(uint8_t* packet) {
	packet[ATEM_INDEX_FLAGS] &= ATEM_MASK_LEN_HIGH;
}

// Sets ATEM flags in packet
void atem_header_flags_set(uint8_t* packet, uint8_t flags) {
	packet[ATEM_INDEX_FLAGS] |= flags;
}

// Gets ATEM flags from packet
uint8_t atem_header_flags_get(uint8_t* packet) {
	return packet[ATEM_INDEX_FLAGS] & ~ATEM_MASK_LEN_HIGH;
}

// Ensures ATEM packet has all required flags set without any illegal flag
void atem_header_flags_get_verify(uint8_t* packet, uint8_t requiredFlags, uint8_t optionalFlags) {
	uint8_t flags = atem_header_flags_get(packet);

	// Verifies required flags are all set
	if ((flags & requiredFlags) != requiredFlags) {
		fprintf(stderr, "Missing required flag(s) 0x%02x from 0x%02x\n", requiredFlags, flags);
		abort();
	}

	// Verifies only required or optional flags are set
	uint8_t illegalFlags = ~(requiredFlags | optionalFlags);
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



// Sets lenght of packet
void atem_header_len_set(uint8_t* packet, uint16_t len) {
	len |= atem_header_flags_get(packet) << 8;
	atem_packet_word_set(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, len);
}

// Gets length of packet
uint16_t atem_header_len_get(uint8_t* packet) {
	uint16_t word = atem_packet_word_get(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW);
	uint16_t len = word & ATEM_MAX_PACKET_LEN;
	if (len < ATEM_LEN_HEADER) {
		fprintf(stderr, "Packet is smaller than minimum size: %d\n", len);
		abort();
	}
	return len;
}

// Ensures length of packet matches expected length
void atem_header_len_get_verify(uint8_t* packet, size_t expectedLen) {
	uint16_t len = atem_header_len_get(packet);
	if (len == expectedLen) return;
	fprintf(stderr, "Expected packet length %zu, but got %d\n", expectedLen, len);
	abort();
}



// Sets session id in packet
void atem_header_sessionid_set(uint8_t* packet, uint16_t sessionId) {
	atem_packet_word_set(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, sessionId);
}

// Gets session id from packet
uint16_t atem_header_sessionid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW);
}

// Ensures session id in packet matches expected session id
void atem_header_sessionid_get_verify(uint8_t* packet, uint16_t expectedSessionId) {
	uint16_t sessionId = atem_header_sessionid_get(packet);
	if (sessionId == expectedSessionId) return;
	fprintf(stderr, "Expected session id 0x%04x, but got 0x%04x\n", expectedSessionId, sessionId);
	abort();
}



// Sets ack id in packet
void atem_header_ackid_set(uint8_t* packet, uint16_t ackId) {
	atem_packet_word_set(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ackId);
}

// Gets ack id from packet
uint16_t atem_header_ackid_get(uint8_t* packet) {
	uint16_t ackId = atem_packet_word_get(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW);
	if (ackId & 0x8000) {
		fprintf(stderr, "Expected ack id to never have MSB set: %04x\n", ackId);
		abort();
	}
	return ackId;
}

// Ensures ack id in packet matches expected ack id
void atem_header_ackid_get_verify(uint8_t* packet, uint16_t expectedAckId) {
	uint16_t ackId = atem_header_ackid_get(packet);
	if (ackId == expectedAckId) return;
	fprintf(stderr, "Expected ack id 0x%04x, but got 0x%04x\n", expectedAckId, ackId);
	abort();
}



// Sets local id in packet
void atem_header_localid_set(uint8_t* packet, uint16_t localId) {
	atem_packet_word_set(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, localId);
}

// Gets local id from packet
uint16_t atem_header_localid_get(uint8_t* packet) {
	uint16_t localId = atem_packet_word_get(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW);
	if (localId & 0x8000) {
		fprintf(stderr, "Expected local id to never have MSB set: %04x\n", localId);
		abort();
	}
	return localId;
}

// Ensures local id in packet matches expected local id
void atem_header_localid_get_verify(uint8_t* packet, uint16_t expectedLocalId) {
	uint16_t localId = atem_header_localid_get(packet);
	if (localId == expectedLocalId) return;
	fprintf(stderr, "Expected local id 0x%04x, but got 0x%04x\n", expectedLocalId, localId);
	abort();
}



// Sets unknown id in packet
void atem_header_unknownid_set(uint8_t* packet, uint16_t unknownId) {
	atem_packet_word_set(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW, unknownId);
}

// Gets unknown id from packet
uint16_t atem_header_unknownid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW);
}

// Ensures unknown id in packet matches expected unknown id
void atem_header_unknownid_get_verify(uint8_t* packet, uint16_t expectedUnknownId) {
	uint16_t unknownId = atem_header_unknownid_get(packet);
	if (unknownId == expectedUnknownId) return;
	fprintf(stderr, "Expected unknown id 0x%04x, but got 0x%04x\n", expectedUnknownId, unknownId);
	abort();
}



// Sets remote id in packet
void atem_header_remoteid_set(uint8_t* packet, uint16_t remoteId) {
	atem_packet_word_set(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, remoteId);
}

// Gets remote id from packet
uint16_t atem_header_remoteid_get(uint8_t* packet) {
	uint16_t remoteId = atem_packet_word_get(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW);
	if (remoteId & 0x8000) {
		fprintf(stderr, "Expected remote id to never have MSB set: %04x\n", remoteId);
		abort();
	}
	return remoteId;
}

// Ensures remote id in packet matches expected unknown id
void atem_header_remoteid_get_verify(uint8_t* packet, uint16_t expectedRemoteId) {
	uint16_t remoteId = atem_header_remoteid_get(packet);
	if (remoteId == expectedRemoteId) return;
	fprintf(stderr, "Expected remote id 0x%04x, but got 0x%04x\n", expectedRemoteId, remoteId);
	abort();
}



// Tests functions in this file
void atem_header_init(void) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	const uint8_t allFlags = 0xff & ~(ATEM_MAX_PACKET_LEN >> 8);

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
	atem_header_flags_set(packet, allFlags);
	assert(atem_header_flags_get(packet) == allFlags);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);

	// Tests atem_header_flags_get_verify
	atem_packet_clear(packet);
	atem_header_flags_get_verify(packet, 0, 0);
	atem_header_flags_set(packet, allFlags);
	atem_header_flags_get_verify(packet, allFlags, 0);
	atem_header_flags_get_verify(packet, 0, allFlags);

	// Tests atem_header_flags_clear
	atem_packet_clear(packet);
	assert(atem_header_flags_get(packet) == 0);
	atem_header_flags_set(packet, allFlags);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_flags_get(packet) == allFlags);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);
	atem_header_flags_clear(packet);
	assert(atem_header_flags_get(packet) == 0x00);

	// Tests atem_header_flags_get with max length
	atem_packet_clear(packet);
	assert(atem_packet_word_get(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW) == 0);
	atem_header_len_set(packet, ATEM_MAX_PACKET_LEN);
	assert(atem_header_len_get(packet) == ATEM_MAX_PACKET_LEN);
	assert(atem_header_flags_get(packet) == 0);

	// Tests atem_header_len_get and atem_header_len_set
	atem_packet_clear(packet);
	atem_header_len_set(packet, ATEM_LEN_HEADER);
	assert(atem_header_len_get(packet) == ATEM_LEN_HEADER);
	atem_header_len_set(packet, 100);
	assert(atem_header_len_get(packet) == 100);

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
