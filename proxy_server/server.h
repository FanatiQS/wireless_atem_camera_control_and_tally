// Include guard
#ifndef SERVER_H
#define SERVER_H

#include <stdint.h> // uint8_t, uint16_t

#include "./udp.h"

// Structure for packets awaiting acknowledgement
typedef struct packet_t {
	struct packet_t* next;
	struct packet_t* previous;
	struct sockaddr sockAddr;
	socklen_t sockLen;
	struct timeval timestamp;
	uint8_t resendsLeft;
	uint16_t bufLen;
	uint8_t buf[0];
} packet_t;

// Structure for proxy connections
typedef struct session_t {
	struct session_t* next;
	struct session_t* previous;
	uint16_t id;
	uint16_t remote;
	struct sockaddr sockAddr;
	socklen_t sockLen;
} session_t;

void broadcastAtemCommands(const uint8_t* commands, const uint16_t len);

// End include guard
#endif
