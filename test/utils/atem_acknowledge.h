// Include guard
#ifndef ATEM_ACKNOWLEDGE_H
#define ATEM_ACKNOWLEDGE_H

#include <stdint.h> // uint8_t, uint16_t

void atem_acknowledge_request_set(uint8_t* packet, uint16_t sessionId, uint16_t remoteId);
uint16_t atem_acknowledge_request_get(uint8_t* packet, uint16_t sessionId);
void atem_acknowledge_request_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t remoteId);
void atem_acknowledge_request_send(int sock, uint16_t sessionId, uint16_t remoteId);
uint16_t atem_acknowledge_request_recv(int sock, uint16_t sessionId);
void atem_acknowledge_request_recv_verify(int sock, uint16_t sessionId, uint16_t remoteId);

void atem_acknowledge_response_set(uint8_t* packet, uint16_t sessionId, uint16_t ackId);
uint16_t atem_acknowledge_response_get(uint8_t* packet, uint16_t sessionId);
void atem_acknowledge_response_get_verify(uint8_t* packet, uint16_t sessionId, uint16_t ackId);
void atem_acknowledge_response_send(int sock, uint16_t sessionId, uint16_t ackId);
uint16_t atem_acknowledge_response_recv(int sock, uint16_t sessionId);
void atem_acknowledge_response_recv_verify(int sock, uint16_t sessionId, uint16_t ackId);

void atem_acknowledge_init(void);

#endif // ATEM_ACKNOWLEDGE_H
