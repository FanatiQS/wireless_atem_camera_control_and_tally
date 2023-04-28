#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stdint.h> // uint8_t, uint16_t

void atem_ack_set(uint8_t* packet, uint16_t sessionId, uint16_t ackId);
uint16_t atem_ack_get(uint8_t* packet, uint16_t sessionId);
void atem_ack_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t ackId);

void atem_ack_send(int sock, uint16_t sessionId, uint16_t ackId);
uint16_t atem_ack_recv(int sock, uint16_t sessionId);
void atem_ack_recv_verify(int sock, uint16_t sessionId, uint16_t ackId);

void atem_ping_set(uint8_t* packet, uint16_t sessionId, uint16_t remoteId);
uint16_t atem_ping_get(uint8_t* packet, uint16_t sessionId);
void atem_ping_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t remoteId);

void atem_ping_send(int sock, uint16_t sessionId, uint16_t remoteId);
uint16_t atem_ping_recv(int sock, uint16_t sessionId);
void atem_ping_recv_verify(int sock, uint16_t sessionId, uint16_t remoteId);

#endif // PAYLOAD_H
