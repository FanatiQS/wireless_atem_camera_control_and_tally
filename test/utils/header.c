#include <stdint.h> // uint8_t, uint16_t
#include <assert.h> // assert

#include "../../src/atem.h" // ATEM_MAX_PACKET_LEN
#include "../../src/atem_private.h" // ATEM_MASK_LEN_HIGH, ATEM_INDEX_FLAGS, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW
#include "./logs.h" // print_debug
#include "./runner.h" // testrunner_abort



// Sets 16bit word in packet
void atem_packet_word_set(uint8_t* packet, uint8_t high, uint8_t low, uint16_t word) {
	packet[high] = (uint8_t)(word >> 8);
	packet[low] = (uint8_t)(word & 0xff);
}

// Gets 16bit word from packet
uint16_t atem_packet_word_get(uint8_t* packet, uint8_t high, uint8_t low) {
	return (packet[high] << 8) | packet[low];
}



// Sets ATEM flags in packet
void atem_header_flags_set(uint8_t* packet, uint8_t flags) {
	packet[ATEM_INDEX_FLAGS] |= flags;
}

// Gets ATEM flags from packet
uint8_t atem_header_flags_get(uint8_t* packet) {
	return packet[ATEM_INDEX_FLAGS] & ~ATEM_MASK_LEN_HIGH;
}

// Verifies all ATEM flags in packet
void atem_header_flags_verify(uint8_t* packet, uint8_t requiredFlags, uint8_t optionalFlags) {
	uint8_t flags = atem_header_flags_get(packet);

	// Verifies required flags are all set
	if (requiredFlags && !((flags & requiredFlags) == requiredFlags)) {
		print_debug("Missing required flag(s) 0x%02x from 0x%02x\n", requiredFlags, flags);
		testrunner_abort();
	}

	// Verifies only required or optional flags are set
	uint8_t illegalFlags = ~(requiredFlags | optionalFlags);
	if (flags & illegalFlags) {
		print_debug("Unexpected illegal flag(s) 0x%02x\n", illegalFlags & flags);
		testrunner_abort();
	}
}



// Verifies that specified ATEM flags are set
void atem_header_flags_isset(uint8_t* packet, uint8_t flags) {
	atem_header_flags_verify(packet, flags, ~flags);
}

// Verifies that specified ATEM flags are clear
void atem_header_flags_isnotset(uint8_t* packet, uint8_t flags) {
	atem_header_flags_verify(packet, 0, ~flags);
}



// Sets lenght of packet
void atem_header_len_set(uint8_t* packet, uint16_t len) {
	len |= atem_header_flags_get(packet) << 8;
	atem_packet_word_set(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, len);
}

// Gets length of packet
uint16_t atem_header_len_get(uint8_t* packet) {
	uint16_t word = atem_packet_word_get(packet, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW);
	return word & ATEM_MAX_PACKET_LEN;
}

// Verifies length of packet
void atem_header_len_verify(uint8_t* packet, uint16_t expectedLen) {
	uint16_t len = atem_header_len_get(packet);
	if (len == expectedLen) return;
	print_debug("Expected packet length %d, but got %d\n", expectedLen, len);
	testrunner_abort();
}



// Sets session id in packet
void atem_header_sessionid_set(uint8_t* packet, uint16_t sessionId) {
	atem_packet_word_set(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, sessionId);
}

// Gets session id from packet
uint16_t atem_header_sessionid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW);
}

// Verifies session id in packet
void atem_header_sessionid_verify(uint8_t* packet, uint16_t expectedSessionId) {
	uint16_t sessionId = atem_header_sessionid_get(packet);
	if (sessionId == expectedSessionId) return;
	print_debug("Expected session id 0x%04x, but got 0x%04x\n", expectedSessionId, sessionId);
	testrunner_abort();
}



// Sets ack id in packet
void atem_header_ackid_set(uint8_t* packet, uint16_t ackId) {
	atem_packet_word_set(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ackId);
}

// Gets ack id from packet
uint16_t atem_header_ackid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW);
}

// Verifies ack id in packet
void atem_header_ackid_verify(uint8_t* packet, uint16_t expectedAckId) {
	uint16_t ackId = atem_header_ackid_get(packet);
	if (ackId == expectedAckId) return;
	print_debug("Expected ack id 0x%04x, but got 0x%04x\n", expectedAckId, ackId);
	testrunner_abort();
}



// Sets local id in packet
void atem_header_localid_set(uint8_t* packet, uint16_t localId) {
	atem_packet_word_set(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW, localId);
}

// Gets local id from packet
uint16_t atem_header_localid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW);
}

// Verifies local id in packet
void atem_header_localid_verify(uint8_t* packet, uint16_t expectedLocalId) {
	uint16_t localId = atem_header_localid_get(packet);
	if (localId == expectedLocalId) return;
	print_debug("Expected local id 0x%04x, but got 0x%04x\n", expectedLocalId, localId);
	testrunner_abort();
}



// Sets unknown id in packet
void atem_header_unknownid_set(uint8_t* packet, uint16_t unknownId) {
	atem_packet_word_set(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW, unknownId);
}

// Gets unknown id from packet
uint16_t atem_header_unknownid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_UNKNOWNID_HIGH, ATEM_INDEX_UNKNOWNID_LOW);
}

// Verifies unknown id in packet
void atem_header_unknownid_verify(uint8_t* packet, uint16_t expectedUnknownId) {
	uint16_t unknownId = atem_header_unknownid_get(packet);
	if (unknownId == expectedUnknownId) return;
	print_debug("Expected unknown id 0x%04x, but got 0x%04x\n", expectedUnknownId, unknownId);
	testrunner_abort();
}



// Sets remote id in packet
void atem_header_remoteid_set(uint8_t* packet, uint16_t remoteId) {
	atem_packet_word_set(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, remoteId);
}

// Gets remote id from packet
uint16_t atem_header_remoteid_get(uint8_t* packet) {
	return atem_packet_word_get(packet, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW);
}

// Verifies remote id in packet
void atem_header_remoteid_verify(uint8_t* packet, uint16_t expectedRemoteId) {
	uint16_t remoteId = atem_header_remoteid_get(packet);
	if (remoteId == expectedRemoteId) return;
	print_debug("Expected remote id between 0x%04x, but got 0x%04x\n", expectedRemoteId, remoteId);
	testrunner_abort();
}
