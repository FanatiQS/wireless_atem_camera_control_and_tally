#ifndef HEADER_H
#define HEADER_H

#include <stdint.h> // uint8_t, uint16_t

void atem_packet_word_set(uint8_t* packet, uint8_t high, uint8_t low, uint16_t word);
uint16_t atem_packet_word_get(uint8_t* packet, uint8_t high, uint8_t low);

void atem_header_flags_set(uint8_t* packet, uint8_t flags);
uint8_t atem_header_flags_get(uint8_t* packet);
void atem_header_flags_verify(uint8_t* packet, uint8_t requiredFlags, uint8_t optionalFlags);

void atem_header_flags_isset(uint8_t* packet, uint8_t flags);
void atem_header_flags_isnotset(uint8_t* packet, uint8_t flags);

void atem_header_len_set(uint8_t* packet, uint16_t len);
uint16_t atem_header_len_get(uint8_t* packet);
void atem_header_len_verify(uint8_t* packet, uint16_t expectedLen);

void atem_header_sessionid_set(uint8_t* packet, uint16_t sessionId);
uint16_t atem_header_sessionid_get(uint8_t* packet);
void atem_header_sessionid_verify(uint8_t* packet, uint16_t expectedSessionId);

void atem_header_ackid_set(uint8_t* packet, uint16_t ackId);
uint16_t atem_header_ackid_get(uint8_t* packet);
void atem_header_ackid_verify(uint8_t* packet, uint16_t expectedAckId);

void atem_header_localid_set(uint8_t* packet, uint16_t localId);
uint16_t atem_header_localid_get(uint8_t* packet);
void atem_header_localid_verify(uint8_t* packet, uint16_t expectLocalId);

void atem_header_unknownid_set(uint8_t* packet, uint16_t unknownId);
uint16_t atem_header_unknownid_get(uint8_t* packet);
void atem_header_unknownid_verify(uint8_t* packet, uint16_t expectedUnknownId);

void atem_header_remoteid_set(uint8_t* packet, uint16_t remoteId);
uint16_t atem_header_remoteid_get(uint8_t* packet);
void atem_header_remoteid_verify(uint8_t* packet, uint16_t expectedRemoteId);

#endif // HEADER_H
