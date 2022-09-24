#include <stdbool.h> // bool, true, false
#include <stdint.h> // uint8_t, uint16_t
#include <stdlib.h> // malloc, NULL, free, abort
#include <stdio.h> // printf, fprintf, stderr, perror
#include <string.h> // memset, memcpy

#include "../src/atem.h"
#include "./udp.h"
#include "./server.h"
#include "./atem_extra.h"
#include "./timer.h"
#include "./debug.h"
#include "./cache.h"



// Configurable limit for number of concurrent connections
#ifndef CONNECTEDSESSIONSLIMIT
#define CONNECTEDSESSIONSLIMIT (SESSIONCHUNKS / 2 * SESSIONCHUNKSIZE)
#endif



int sockProxy;
static uint16_t lastSessionId;
static uint16_t connectedSessions;
static struct sessionChunk_t* sessionReuseChunk;
static struct sessionChunk_t* sessionTable[SESSIONCHUNKS];
static struct session_t* sessionBroadcastList;
static struct packet_t* resendQueueHead;
static struct packet_t* resendQueueTail;



// Creates and opens proxy socket
void setupProxy() {
 	sockProxy = createSocket();
	const struct sockaddr_in sockAddr = createAddr(INADDR_ANY);
	if (bind(sockProxy, (const struct sockaddr *)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to bind socket");
		abort();
	}
}



// Allocates memory for a session chunk or reuses previous unfreed chunk
static struct sessionChunk_t* createSessionChunk(uint8_t high) {
	struct sessionChunk_t* chunk;

	// Reuses session chunk that was not freed
	if (sessionReuseChunk != NULL) {
		chunk = sessionReuseChunk;
		sessionReuseChunk = NULL;
		DEBUG_PRINT("reusing session chunk for 0x%02x\n", high);
	}
	// Allocates memory for a chunk in the session table
	else {
		chunk = (struct sessionChunk_t*)malloc(sizeof(struct sessionChunk_t));
		DEBUG_PRINT("allocated memory for session chunk 0x%02x\n", high);
		if (chunk == NULL) {
			fprintf(stderr, "Failed to create session chunk");
			abort();
		}

		chunk->fillLevel = 0;
		for (uint16_t i = 0; i < SESSIONCHUNKSIZE; i++) {
			chunk->sessions[i] = NULL;
		}
	}

	// Initializes session chunk
	chunk->id = high;
	sessionTable[high] = chunk;
	return chunk;
}

// Allocates memory for an ATEM session
static struct session_t* createSession() {
	return (struct session_t*)malloc(sizeof(struct session_t));
}

// Releases empty session chunk in session table
static void releaseSessionChunk(struct sessionChunk_t* chunk) {
	// Removes it from the session table
	sessionTable[chunk->id] = NULL;

	// Stores empty session chunk for reuse
	if (sessionReuseChunk == NULL) {
		sessionReuseChunk = chunk;
		DEBUG_PRINT("saving session chunk for reuse\n");
	}
	// Frees session chunk if not reusable
	else {
		free(chunk);
		DEBUG_PRINT("released chunk memory\n");
	}
}

// Adds session to chunk in session table
static void addSessionToChunk(struct sessionChunk_t* chunk, uint8_t low, struct session_t* session) {
	chunk->sessions[low] = session;
	chunk->fillLevel++;
}

// Removes session from chunk in session table and releases the chunk if empty
static void removeSessionFromChunk(struct sessionChunk_t* chunk, uint8_t low) {
	chunk->sessions[low] = NULL;
	chunk->fillLevel--;
	if (!chunk->fillLevel) {
		releaseSessionChunk(chunk);
	}
}

// Removes session from session chunk and releases the sessions memory
// Does not remove the session from the sessionBroadcastList
static void releaseSession(struct session_t* session) {
	// Removes session from session chunk
	removeSessionFromChunk(session->chunk, session->id);
	if (session->handshakeChunk != NULL) {
		removeSessionFromChunk(session->handshakeChunk, session->handshakeId);
	}

	// Releases sessions memory
	connectedSessions--;
	free(session);
	DEBUG_PRINT("released session memory\n");
}

// Adds session to broadcast list to receive data
static void enqueueSession(struct session_t* session) {
	if (sessionBroadcastList == NULL) {
		pingTimerEnable();
	}
	else {
		sessionBroadcastList->broadcastPrev = session;
	}
	session->broadcastNext = sessionBroadcastList;
	session->broadcastPrev = NULL;
	sessionBroadcastList = session;

	DEBUG_PRINT("added session 0x%02x%02x to broadcast list\n", session->chunk->id, session->id);
}

// Removes session from broadcast list
static void dequeueSession(struct session_t* session) {
	if (session->broadcastPrev == NULL) {
		sessionBroadcastList = session->broadcastNext;
		if (sessionBroadcastList == NULL) {
			pingTimerDisable();
		}
	}
	else {
		session->broadcastPrev->broadcastNext = session->broadcastNext;
	}
	if (session->broadcastNext != NULL) {
		session->broadcastNext->broadcastPrev = session->broadcastPrev;
	}

	DEBUG_PRINT("removed session 0x%02x%02x from broadcast list\n", session->chunk->id, session->id);
}



// Sends an ATEM buffer to an address using existing UDP connection
static void sendBuffer(uint8_t* buf, uint16_t len, struct sockaddr* sockAddr, socklen_t sockLen) {
	DEBUG_PRINT_BUFFER(buf, len, "sending data");

	if (sendto(sockProxy, buf, len, 0, sockAddr, sockLen) == -1) {
		perror("Failed to send proxy data");
	}
}

// Sends reject packet to session indicating opening handshake failed
static void sendRejectPacket(uint8_t high, uint8_t low, struct sockaddr* sockAddr, socklen_t sockLen) {
	uint8_t buf[ATEM_LEN_SYN] = {
		[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
		[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
		[ATEM_INDEX_SESSION_HIGH] = high,
		[ATEM_INDEX_SESSION_LOW] = low,
		[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_REJECTED
	};
	sendBuffer(buf, ATEM_LEN_SYN, sockAddr, sockLen);
}



// Creates an ATEM packet
// It has to be added to the sessions packet queue, either with pushPacket or manually
static struct packet_t* createPacket(struct session_t* session, uint16_t len, uint8_t high, uint8_t low) {
	// Allocates memory for the packet
	struct packet_t* packet = (struct packet_t*)malloc(sizeof(struct packet_t) + len);
	if (packet == NULL) {
		fprintf(stderr, "Failed to create packet with buffer size %d\n", len);
		abort();
	}

	// Initializes the packet content to the bare minimum
	packet->session = session;
	packet->remainingResends = ATEM_RESEND_OPENINGHANDSHAKE;
	packet->len = len;
	memset(packet->buf, 0, len);
	packet->buf[ATEM_INDEX_LEN_LOW] = len;
	packet->buf[ATEM_INDEX_SESSION_HIGH] = high;
	packet->buf[ATEM_INDEX_SESSION_LOW] = low;

	DEBUG_PRINT("allocated memory for packet\n");

	return packet;
}

// Pushes ATEM packet to the sessions internal packet queue
static void pushPacket(struct session_t* session, struct packet_t* packet) {
	if (session->localPacketHead == NULL) {
		session->localPacketHead = packet;
	}
	else {
		session->localPacketTail->localNext = packet;
	}
	session->localPacketTail = packet;
	packet->localNext = NULL;
}

// Sets packets remote id and increments it on the session
static void setPacketRemoteId(struct packet_t* packet) {
	struct session_t* session = packet->session;
	session->remoteId++;
	session->remoteId &= 0x7fff;
	packet->buf[ATEM_INDEX_REMOTEID_HIGH] = session->remoteId >> 8;
	packet->buf[ATEM_INDEX_REMOTEID_LOW] = session->remoteId & 0xff;
}

// Adds the packet to the global resend queue
static void enqueuePacket(struct packet_t* packet) {
	if (resendQueueHead == NULL) {
		resendQueueHead = packet;
		resendTimerAdd(&packet->resendTimer);
	}
	else {
		resendQueueTail->resendNext = packet;
	}
	packet->resendNext = NULL;
	packet->resendPrev = resendQueueTail;
	resendQueueTail = packet;
}

// Removes the packet from the global resend queue
static void dequeuePacket(struct packet_t* packet) {
	if (packet->resendPrev == NULL) {
		resendQueueHead = packet->resendNext;
		resendTimerRemove((resendQueueHead) ? &resendQueueHead->resendTimer : NULL);
	}
	else {
		packet->resendPrev->resendNext = packet->resendNext;
		if (packet->lastInChunk == true) {
			packet->resendPrev->lastInChunk = true;
		}
	}
	if (packet->resendNext == NULL) {
		resendQueueTail = packet->resendPrev;
	}
	else {
		packet->resendNext->resendPrev = packet->resendPrev;
	}
}

// Sends an ATEM packet and adds it to the resend queue
static void sendPacket(struct packet_t* packet, bool isRetransmit) {
	// Sets resend flag if this packet is being resent
	if (isRetransmit) {
		packet->buf[ATEM_INDEX_FLAGS] |= ATEM_FLAG_RETRANSMIT;
	}

	// Sends and enqueues packet
	sendBuffer(packet->buf, packet->len, &packet->session->sockAddr, packet->session->sockLen);
	setTimeout(&packet->resendTimer, ATEM_RESEND_TIME);
	enqueuePacket(packet);
}

// Removes packet from global resend queue and releases its memory
static void releasePacket(struct packet_t* packet) {
	DEBUG_PRINT("releasing packet 0x%02x%02x for session 0x%02x%02x\n",
		packet->buf[ATEM_INDEX_REMOTEID_HIGH], packet->buf[ATEM_INDEX_REMOTEID_LOW],
		packet->buf[ATEM_INDEX_SESSION_HIGH], packet->buf[ATEM_INDEX_SESSION_LOW]
	);

	dequeuePacket(packet);
	free(packet);
	DEBUG_PRINT("released packet memory\n");
}

// Removes and releases all packets from a session
static void flushPackets(struct session_t* session) {
	struct packet_t* p;
	while ((p = session->localPacketHead) != NULL) {
		session->localPacketHead = p->localNext;
		releasePacket(p);
	}
}

// Creates and sends a closing packet
// Sending a closing packet assumes the session has no packets in its local packet queue
static void sendClosingPacket(struct session_t* session) {
	// Creates ATEM packet
	struct packet_t* packet = createPacket(session, ATEM_LEN_SYN, session->chunk->id, session->id);

	// Sets session to only handle closing handshake
	session->closed = true;

	// Initializes packet
	packet->remainingResends = ATEM_RESEND_CLOSINGHANDSHAKE;
	packet->lastInChunk = true;

	// Sets packet to only packet in sessions local queue
	packet->localNext = NULL;
	session->localPacketHead = packet;
	session->localPacketTail = packet;

	// Sets packets buffer
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSING;

	// Sends packet
	sendPacket(packet, false);
}

// Resends a packet
static void resendPacket(struct packet_t* packet) {
	struct session_t* session = packet->session;

	// Resends the packet if it has not reached the limit
	if (packet->remainingResends > 0) {
		packet->remainingResends--;
		dequeuePacket(packet);
		sendPacket(packet, true);
		DEBUG_PRINT("resending packet 0x%02x%02x for session 0x%02x%02x\n",
			packet->buf[ATEM_INDEX_REMOTEID_HIGH], packet->buf[ATEM_INDEX_REMOTEID_LOW],
			session->chunk->id, session->id
		);
		return;
	}

	// Requests closing session after resends run out
	if (!session->closed) {
		// Removes and releases all of sessions local packets
		flushPackets(session);
		sendClosingPacket(session);

		// Removes session from broadcast list
		if (session->handshakeChunk == NULL) {
			dequeueSession(session);
		}
		// Removes session from handshake chunk if handshake is not complete
		else {
			removeSessionFromChunk(session->handshakeChunk, session->handshakeId);
			session->handshakeChunk = NULL;
		}

		DEBUG_PRINT("closing session 0x%02x%02x for not responding\n", session->chunk->id, session->id);
	}
	// Removes and releases session after closing packet resends run out
	else {
		releasePacket(packet);
		releaseSession(session);
		printf("Closed session 0x%02x%02x, dropped\n", session->chunk->id, session->id);
	}
}

// Acknowledges and releases the sessions next locally queued packet
static void acknowledgeNextPacket(struct session_t* session) {
	struct packet_t* packet = session->localPacketHead;
	session->localPacketHead = packet->localNext;
	DEBUG_PRINT("acknowledged packet 0x%02x%02x for session 0x%02x%02x\n",
		packet->buf[ATEM_INDEX_REMOTEID_HIGH], packet->buf[ATEM_INDEX_REMOTEID_LOW],
		packet->buf[ATEM_INDEX_SESSION_HIGH], packet->buf[ATEM_INDEX_SESSION_LOW]
	);
	releasePacket(packet);
}

// Creates and sends an ATEM packet containing commands in the payload
static void sendDataPacket(struct session_t* session, uint8_t* commands, uint16_t len) {
	// Creates ATEM packet
	struct packet_t* packet = createPacket(session, len, session->chunk->id, session->id);

	// Adds packet to the sessions local queue
	pushPacket(session, packet);

	// Sets packets buffer
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST | len >> 8;
	setPacketRemoteId(packet);
	memcpy(packet->buf + ATEM_LEN_HEADER, commands, len);

	// Sends packet
	sendPacket(packet, false);

	DEBUG_PRINT("sending data packet\n");
}

// Sends ATEM commands to a proxy session
void sendAtemCommands(struct session_t* session, uint8_t* commands, uint16_t len) {
	sendDataPacket(session, commands, len + ATEM_LEN_HEADER);
	resendQueueTail->lastInChunk = true;
}

// Sends ATEM commands to all proxy sessions
void broadcastAtemCommands(uint8_t* commands, uint16_t len) {
	if (sessionBroadcastList == NULL) return;
	len += ATEM_LEN_HEADER;
	for (
		struct session_t* session = sessionBroadcastList;
		session != NULL;
		session = session->broadcastNext
	) {
		sendDataPacket(session, commands, len);
		resendQueueTail->lastInChunk = false;
	}
	resendQueueTail->lastInChunk = true;
}



// Gets session matching session id from session table
static struct session_t* getSession(uint8_t high, uint8_t low) {
	struct sessionChunk_t* chunk = sessionTable[high];
	return (chunk) ? chunk->sessions[low] : NULL;
}

// Starts an opening handshake procedure
static void startHandshake(uint8_t high, uint8_t low, struct sockaddr* sockAddr, socklen_t sockLen) {
	// Rejects session if sessionTable is already known to be full
	if (connectedSessions == CONNECTEDSESSIONSLIMIT) {
		sendRejectPacket(high, low, sockAddr, sockLen);
		printf("Server can not handle more sessions\n");
		return;
	}

	// Gets the session chunk the session should be in based on the high byte of the session id
	struct sessionChunk_t* handshakeChunk = sessionTable[high];

	// Creates the session chunk if it did not exist already
	if (handshakeChunk == NULL) {
		handshakeChunk = createSessionChunk(high);
	}
	// Resends handshake response if session already exists
	else if (handshakeChunk->sessions[low]) {
		DEBUG_PRINT("resends handshake SYNACK for session 0x%02x%02x\n", high, low);
		resendPacket(handshakeChunk->sessions[low]->localPacketHead); // should it resend, ignore or send extra?
		return;
	}

	// Creates and initializes the session
	struct session_t* session = createSession();
	if (session == NULL) {
		fprintf(stderr, "Failed to create session for 0x%02x%02x\n", high, low);
		abort();
	}
	connectedSessions++;
	addSessionToChunk(handshakeChunk, low, session);
	session->handshakeChunk = handshakeChunk;
	session->handshakeId = low;
	session->sockAddr = *sockAddr;
	session->sockLen = sockLen;
	session->closed = false;
	session->remoteId = 0;

	// Sets server assigned session id
	struct sessionChunk_t* serverChunk;
	uint8_t sessionIdHigh;
	do {
		lastSessionId++;
		lastSessionId &= 0x7fff;
		sessionIdHigh = lastSessionId >> 8 | 0x80;
		serverChunk = sessionTable[sessionIdHigh];
		session->id = lastSessionId & 0xff;
	} while (serverChunk && serverChunk->sessions[session->id]);

	// Adds session to server session chunk
	if (serverChunk == NULL) {
		serverChunk = createSessionChunk(sessionIdHigh);
	}
	addSessionToChunk(serverChunk, session->id, session);
	session->chunk = serverChunk;

	// Creates and sends a SYNACK as a response to the SYN
	struct packet_t* packet = createPacket(session, ATEM_LEN_SYN, high, low);
	session->localPacketHead = packet;
	session->localPacketTail = packet;
	packet->localNext = NULL;
	packet->lastInChunk = true;
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_SUCCESS;
	packet->buf[ATEM_INDEX_NEW_SESSION_HIGH] = sessionIdHigh & 0x7f;
	packet->buf[ATEM_INDEX_NEW_SESSION_LOW] = session->id;
	sendPacket(packet, false);

	// Logs information about session
	printf(
		"Session created\n"
		"\tConnected with session id 0x%02x%02x\n"
		"\tAssigned session id 0x%02x%02x\n"
		"\tFrom %s:%d\n",
		high, low,
		sessionIdHigh, session->id,
		inet_ntoa(((struct sockaddr_in*)sockAddr)->sin_addr),
		((struct sockaddr_in*)sockAddr)->sin_port
	);
}

// Completes the sessions opening handshake
static void completeHandshake(uint8_t high, uint8_t low) {
	// Gets session to upgrade from sessions table
	struct sessionChunk_t* handshakeChunk = sessionTable[high];
	struct session_t* session;
	if (handshakeChunk == NULL || (session = handshakeChunk->sessions[low]) == NULL) {
		fprintf(stderr, "Session 0x%02x%02x did not exist in sessions table\n", high, low);
		return;
	}

	// Completes handshake
	acknowledgeNextPacket(session);
	removeSessionFromChunk(handshakeChunk, low);
	session->handshakeChunk = NULL;
	enqueueSession(session);
	dumpAtemData(session);

	DEBUG_PRINT("session handshake complete for 0x%02x%02x\n", high, low);
}

// Initiates closing handshake by client
static void closeOpenSession(struct session_t* session) {
	// Removes session from broadcast list and flushes its packets
	if (session->closed == false && session->handshakeChunk == NULL) {
		dequeueSession(session);
	}
	flushPackets(session);

	// Sends close response
	uint8_t buf[ATEM_LEN_SYN] = {
		[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
		[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
		[ATEM_INDEX_SESSION_HIGH] = session->chunk->id,
		[ATEM_INDEX_SESSION_LOW] = session->id,
		[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSED
	};
	sendBuffer(buf, ATEM_LEN_SYN, &session->sockAddr, session->sockLen);

	// Release session memory
	releaseSession(session);

	printf("Closed session 0x%02x%02x initiated by client\n", session->chunk->id, session->id);
}

// Completes close handshake initiated by server
static void closeClosingSession(struct session_t* session) {
	if (session->closed == false) {
		fprintf(stderr, "Ignoring close response to to non-sent request\n");
		return;
	}
	releasePacket(session->localPacketHead);
	releaseSession(session);
	printf("Closed session 0x%02x%02x, initiated by server\n", session->chunk->id, session->id);
}

// Acknowledges and releases sessions packets up to remote id
static void acknowledgeLocalPacket(struct session_t* session, uint8_t remoteHigh, uint8_t remoteLow) {
	struct packet_t* packet;
	while (
		(packet = session->localPacketHead) != NULL &&
		((packet->buf[ATEM_INDEX_REMOTEID_HIGH] < remoteHigh ||
		(packet->buf[ATEM_INDEX_REMOTEID_HIGH] == remoteHigh &&
		packet->buf[ATEM_INDEX_REMOTEID_LOW] <= remoteLow)))
	) {
		acknowledgeNextPacket(session);
	}
}

// Responds to acknowledge request
static void acknowledgeRemotePacket(struct session_t* session, uint8_t localHigh, uint8_t localLow) {
	printf("HANDLING REMOTE PACKETS IS CURRENTLY UNSUPPORTED\n");
}



#define HAS_FLAGS(buf, flags) (buf[ATEM_INDEX_FLAGS] & flags)
#define IS_OPCODE(buf, opcode) (buf[ATEM_INDEX_OPCODE] == opcode)

// Processes ATEM packets from client to proxy server
void processProxyData() {
	DEBUG_PRINT("started receiving data from proxy socket\n");

	// Reads UDP data proxy socket
	uint8_t buf[ATEM_MAX_PACKET_LEN];
	struct sockaddr sockAddr;
	socklen_t sockLen = sizeof(sockAddr);
	ssize_t recvLen = recvfrom(sockProxy, buf, ATEM_MAX_PACKET_LEN, 0, &sockAddr, &sockLen);
	if (recvLen == -1) {
		perror("Failed to read proxy data");
		return;
	}

	// Prints received buffer when debug printing is enabled
	DEBUG_PRINT_BUFFER(buf, recvLen,
		"received data from %s:%d",
		inet_ntoa(((struct sockaddr_in*)&sockAddr)->sin_addr),
		((struct sockaddr_in*)&sockAddr)->sin_port
	);

	// Processes opening handshake
	if (!(buf[ATEM_INDEX_SESSION_HIGH] & 0x80)) {
		if (HAS_FLAGS(buf, ATEM_FLAG_SYN)) {
			if (recvLen != ATEM_LEN_SYN) {
				fprintf(stderr,
					"Received SYN packet for unconnected session was %zd bytes long\n",
					recvLen
				);
				return;
			}
			if (!IS_OPCODE(buf, ATEM_CONNECTION_OPENING)) {
				fprintf(stderr,
					"Received SYN packet for unconnected session had an invalid opcode %02x\n",
					buf[ATEM_INDEX_FLAGS]
				);
				return;
			}
			startHandshake(buf[ATEM_INDEX_SESSION_HIGH], buf[ATEM_INDEX_SESSION_LOW], &sockAddr, sockLen);
		}
		else if (HAS_FLAGS(buf, ATEM_FLAG_ACK)) {
			if (recvLen != ATEM_LEN_HEADER) {
				fprintf(stderr,
					"Received ACK packet for unconnected session was %zd bytes long\n",
					recvLen
				);
				return;
			}
			completeHandshake(buf[ATEM_INDEX_SESSION_HIGH], buf[ATEM_INDEX_SESSION_LOW]);
		}
		return;
	}

	// Aborts processing packets without protocol header
	if (recvLen < ATEM_LEN_HEADER) {
		fprintf(stderr, "Packet was %zd bytes long and could not to contain a header\n", recvLen);
		return;
	}

	// Gets session from session table
	struct session_t* session = getSession(buf[ATEM_INDEX_SESSION_HIGH], buf[ATEM_INDEX_SESSION_LOW]);

	// Aborts if session id does not exist
	if (session == NULL) {
		fprintf(stderr,
			"Session 0x%02x%02x did not exist\n",
			buf[ATEM_INDEX_SESSION_HIGH], buf[ATEM_INDEX_SESSION_LOW]
		);
		return;
	}

	// Processes closing handshake
	if (HAS_FLAGS(buf, ATEM_FLAG_SYN)) {
		if (IS_OPCODE(buf, ATEM_CONNECTION_CLOSING)) {
			closeOpenSession(session);
		}
		else if (IS_OPCODE(buf, ATEM_CONNECTION_CLOSED)) {
			closeClosingSession(session);
		}
		else {
			fprintf(stderr,
				"Received SYN packet for connected session had an invalid opcode %02x\n",
				buf[ATEM_INDEX_FLAGS]
			);
		}
		return;
	}

	// Does not process packets after closing handshake has started
	if (session->closed) {
		fprintf(stderr,
			"Ignoring packet for closing session 0x%02x%02x\n",
			session->chunk->id, session->id
		);
		return;
	}

	// Processes acknowledgements for previously sent packets
	if (HAS_FLAGS(buf, ATEM_FLAG_ACK)) {
		acknowledgeLocalPacket(session, buf[ATEM_INDEX_ACKID_HIGH], buf[ATEM_INDEX_ACKID_LOW]);
	}

	// Processes acknowledge requests
	if (HAS_FLAGS(buf, ATEM_FLAG_ACKREQUEST)) {
		acknowledgeRemotePacket(session, buf[ATEM_INDEX_REMOTEID_HIGH], buf[ATEM_INDEX_REMOTEID_LOW]);
	}
}

// Resends all packets in chunk
void resendProxyPackets() {
	do {
		resendPacket(resendQueueHead);
	} while (resendQueueHead != NULL && !resendQueueHead->lastInChunk);
}

// Sends ping packets to all connected sessions
void pingProxySessions() {
	DEBUG_PRINT("sending pings to connected sessions\n");

	// Sends ping packets to every connected session
	if (sessionBroadcastList == NULL) return;
	for (
		struct session_t* session = sessionBroadcastList;
		session != NULL;
		session = session->broadcastNext
	) {
		// Creates ATEM packet
		struct packet_t* packet = createPacket(session, ATEM_LEN_HEADER, session->chunk->id, session->id);

		// Adds packet to the session local queue and make it part of a chunk
		pushPacket(session, packet);
		packet->lastInChunk = false;

		// Sets packet buffer
		packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST;
		setPacketRemoteId(packet);

		// Sends packet
		sendPacket(packet, false);

		DEBUG_PRINT("sent ping to session 0x%02x%02x\n", session->chunk->id, session->id);
	}

	// Ends resend packet queue chunk
	resendQueueTail->lastInChunk = true;

	// Restarts ping timer
	pingTimerRestart();
}

