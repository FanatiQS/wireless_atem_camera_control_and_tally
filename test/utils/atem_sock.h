// Include guard
#ifndef ATEM_SOCKET_H
#define ATEM_SOCKET_H

#include <stdint.h> // uint8_t
#include <stddef.h> // size_t

#include <netinet/in.h> // struct sockaddr_in

void atem_packet_verify(uint8_t* packet, size_t recvLen);

int atem_socket_create(void);
void atem_socket_close(int sock);

void atem_socket_connect(int sock);
struct sockaddr_in atem_socket_listen(int sock, uint8_t* packet);

void atem_socket_send(int sock, uint8_t* packet);
void atem_socket_recv(int sock, uint8_t* packet);
void atem_socket_recv_print(int sock);

void atem_socket_norecv(int sock);

#endif // ATEM_SOCKET_H
