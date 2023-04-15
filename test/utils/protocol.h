#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h> // uint8_t, uint16_t

#include <sys/socket.h> // ssize_t

extern char* atemServerAddr;

void atem_packet_clear(uint8_t* packet);
void atem_packet_verify(uint8_t* packet, ssize_t recvLen);

int atem_socket_init();
void atem_socket_close(int sock);

void atem_socket_connect(int sock);
uint16_t atem_socket_listen(int sock, uint8_t* packet);
int atem_socket_listen_fresh(uint8_t* packet);

void atem_socket_send(int sock, uint8_t* packet);
void atem_socket_recv(int sock, uint8_t* packet);

void atem_socket_norecv(int sock);

#endif // PROTOCOL_H
