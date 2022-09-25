// Include guard
#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h> // bool
#include <stdint.h> // uint8_t, uint16_t
#include <time.h> // timespec

#include "./udp.h" // sockaddr, socklen_t

// Structure for packets awaiting acknowledgement
struct packet_t {
	struct packet_t* localNext;
	struct packet_t* resendNext;
	struct packet_t* resendPrev;
	struct session_t* session;
	struct timespec resendTimer;
	uint8_t remainingResends;
	bool lastInChunk;
	uint16_t len;
	uint8_t buf[0];
};

// Structure for proxy connections
struct session_t {
	struct sessionChunk_t* chunk;
	struct sessionChunk_t* handshakeChunk;
	struct session_t* broadcastNext;
	struct session_t* broadcastPrev;
	struct packet_t* localPacketHead;
	struct packet_t* localPacketTail;
	uint8_t handshakeId;
	uint8_t id;
	bool closed;
	uint16_t remoteId;
	struct sockaddr sockAddr;
	socklen_t sockLen;
};

// Structure used in session lookup table to reduce memory footprint
#define SESSIONCHUNKS 256
#define SESSIONCHUNKSIZE 256
struct sessionChunk_t {
	uint8_t id;
	uint8_t fillLevel;
	struct session_t* sessions[SESSIONCHUNKSIZE];
};

extern int sockProxy;
void setupProxy();
void processProxyData();

void sendAtemCommands(struct session_t* session, uint8_t* commands, uint16_t len);
void broadcastAtemCommands(uint8_t* commands, uint16_t len);

void resendProxyPackets();
void pingProxySessions();

// End include guard
#endif
