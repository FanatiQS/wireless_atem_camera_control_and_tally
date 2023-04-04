#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool

void atem_handshake_newsessionid_set(uint8_t* packet, uint16_t sessionId);
uint16_t atem_handshake_newsessionid_get(uint8_t* packet);
void atem_handshake_newsessionid_verify(uint8_t* packet, uint16_t expectedNewSessionId);

void atem_handshake_opcode_set(uint8_t* packet, uint8_t opcode);
uint8_t atem_handshake_opcode_get(uint8_t* packet);
void atem_handshake_opcode_verify(uint8_t* packet, uint8_t expectedOpcode);

void atem_handshake_set(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId);
uint8_t atem_handshake_get(uint8_t* packet, uint16_t sessionId, bool retx);
void atem_handshake_verify(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId);

void atem_handshake_send(int sock, uint8_t opcode, bool retx, uint16_t sessionId);
void atem_handshake_recv(int sock, uint8_t opcode, bool retx, uint16_t sessionId);

#endif // HANDSHAKE_H
