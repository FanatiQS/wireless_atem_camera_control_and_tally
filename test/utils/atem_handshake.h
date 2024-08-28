// Include guard
#ifndef ATEM_HANDSHAKE_H
#define ATEM_HANDSHAKE_H

#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool

void atem_handshake_newsessionid_set(uint8_t* packet, uint16_t newSessionId);
uint16_t atem_handshake_newsessionid_get(uint8_t* packet);
void atem_handshake_newsessionid_get_verify(uint8_t* packet, uint16_t expectedNewSessionId);

void atem_handshake_opcode_set(uint8_t* packet, uint8_t opcode);
uint8_t atem_handshake_opcode_get(uint8_t* packet);
void atem_handshake_opcode_get_verify(uint8_t* packet, uint8_t expectedOpcode);

void atem_handshake_sessionid_set(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId);
uint16_t atem_handshake_sessionid_get(uint8_t* packet, uint8_t opcode, bool retx);
void atem_handshake_sessionid_get_verify(uint8_t* packet, uint8_t opcode, bool retx, uint16_t sessionId);
void atem_handshake_sessionid_send(int sock, uint8_t opcode, bool retx, uint16_t sessionId);
uint16_t atem_handshake_sessionid_recv(int sock, uint8_t opcode, bool retx);
void atem_handshake_sessionid_recv_verify(int sock, uint8_t opcode, bool retx, uint16_t expectedSessionId);

void atem_handshake_newsessionid_send(int sock, uint8_t opcode, bool retx, uint16_t sessionId, uint16_t newSessionId);
uint16_t atem_handshake_newsessionid_recv(int sock, uint8_t opcode, bool retx, uint16_t sessionId);
void atem_handshake_newsessionid_recv_verify(int sock, uint8_t opcode, bool retx, uint16_t sessionId, uint16_t expectedNewSessionId);

void atem_handshake_resetpeer(void);

uint16_t atem_handshake_start_client(int sock, uint16_t sessionId);
uint16_t atem_handshake_start_server(int sock);

uint16_t atem_handshake_connect(int sock, uint16_t sessionId);
uint16_t atem_handshake_tryconnect(int sock, uint16_t clientSessionId);
uint16_t atem_handshake_listen(int sock, uint16_t newSessionId);
void atem_handshake_close(int sock, uint16_t sessionId);

void atem_handshake_fill(int sock);
void atem_handshake_flush(int sock, uint16_t session_id);

void atem_handshake_init(void);

#endif // ATEM_HANDSHAKE_H
