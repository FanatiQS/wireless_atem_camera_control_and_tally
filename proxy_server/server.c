#include <stdio.h> // perror, fprintf, stderr, printf
#include <stdint.h> // uint8_t uint16_t
#include <stdbool.h> // true, bool
#include <string.h> // memset, memcpy
#include <stdlib.h> // malloc, free, NULL, exit, EXIT_FAILURE

#include "../src/atem.h"
#include "./udp.h"
#include "./atem_connection.h"
#include "./server.h"
#include "./atem_extra.h"



//!! only for tmp testing of data transmission
#define ATEM_LEN_VER 12
#define ATEM_INDEX_CMDLEN (ATEM_LEN_HEADER + 1)
#define ATEM_INDEX_VERMAJ (ATEM_LEN_HEADER + 9)
#define ATEM_INDEX_VERMIN (ATEM_INDEX_VERMAJ + 2)

//!! only for testing
void restartAtemConnection();



// Proxy socket for ATEM to communicate with client sockets
int proxySock;

// Creates a proxy socket for clients to connect to
void setupProxyServer() {
 	proxySock = createSocket();
	const struct sockaddr_in proxySockAddr = createAddr(INADDR_ANY);
	if (bind(proxySock, (const struct sockaddr *)&proxySockAddr, sizeof(proxySockAddr))) {
		perror("Failed to bind socket");
		exit(EXIT_FAILURE);
	}
}



// Queue of packets awaiting acknowledgement
packet_t* queueHead;
packet_t* queueTail;

// Creates an ATEM packet for proxy a proxy connection
packet_t* createPacket(uint16_t bufLen, uint16_t sessionId, struct sockaddr sockAddr, socklen_t sockLen) {
	// Creates a packet for proxy connections
	packet_t* packet = (packet_t*)malloc(sizeof(packet_t) + bufLen);

	// Throws on malloc fail
	if (packet == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	// Adds who to send the response to
	packet->sockAddr = sockAddr;
	packet->sockLen = sockLen;

	// Sets number of resends this packet has left to what could be send before ATEM_TIMEOUT
	packet->resendsLeft = (ATEM_TIMEOUT * 1000 / ATEM_RESEND_TIME);

	// Sets timestamp to 0 to indicate it has not been initiated
	packet->timestamp.tv_sec = 0;

	// Sets length and session id of response buffer (length in buffer only up to 255)
	packet->bufLen = bufLen;
	memset(packet->buf, 0, bufLen);
	packet->buf[ATEM_INDEX_LEN] = bufLen;
	packet->buf[ATEM_INDEX_SESSION_HIGH] = sessionId >> 8;
	packet->buf[ATEM_INDEX_SESSION_LOW] = sessionId & 0xff;

	// Returns packet
	return packet;
}

// Sends a packet to a proxy connection and adds it to the queue
void sendPacket(packet_t* packet) {
	// Sets resend flag if this is not the first time the packet has been transmitted
	if (packet->timestamp.tv_sec != 0) {
		packet->buf[0] |= ATEM_FLAG_RETRANSMIT;
	}

	// Sends buffer of packet
	if (sendto(proxySock, packet->buf, packet->bufLen, 0, &packet->sockAddr, packet->sockLen) == -1) {
		perror("Unable to send data to relay socket");
	}

	// Adds the packet to end of linked list for packets in transit
	if (queueHead == NULL) {
		queueHead = packet;
	}
	else {
		queueTail->next = packet;
	}
	packet->next = NULL;
	packet->previous = queueTail;
	queueTail = packet;

	// Updates timestamp of when it was sent
	getTime(&packet->timestamp);
}

// Removes the packet from the queue
void dequeuePacket(packet_t* packet) {
	if (queueHead == packet) {
		queueHead = packet->next;
	}
	else {
		packet->previous->next = packet->next;
	}

	if (queueTail == packet) {
		queueTail = packet->previous;
	}
	else {
		packet->next->previous = packet->previous;
	}
}

// Finds the packet in the queue being acknowledged by the buffer
packet_t* findPacketInQueue(uint8_t* buf) {
	packet_t* packet = queueHead;
	while (packet != NULL) {
		if (
			packet->buf[ATEM_INDEX_SESSION_HIGH] == buf[ATEM_INDEX_SESSION_HIGH] &&
			packet->buf[ATEM_INDEX_SESSION_LOW] == buf[ATEM_INDEX_SESSION_LOW] &&
			packet->buf[ATEM_INDEX_ACKID_HIGH] == buf[ATEM_INDEX_REMOTEID_HIGH] &&
			packet->buf[ATEM_INDEX_ACKID_LOW] == buf[ATEM_INDEX_REMOTEID_LOW]
		) {
			return packet;
		}
		packet = packet->next;
	}
	return NULL;
}



// The session id given to the last proxy connection
uint16_t lastSessionId = 0;

// List of all proxy connections
session_t* sessionsHead;
session_t* sessionsTail;

// Creates and sends an ATEM successfull connection response packet
void sendConnectPacket(uint16_t sessionId, struct sockaddr sockAddr, socklen_t sockLen) {
	uint16_t newSessionId = ++lastSessionId % 0x7fff;
	packet_t* packet = createPacket(ATEM_LEN_SYN, sessionId, sockAddr, sockLen);
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_SUCCESS;
	packet->buf[ATEM_INDEX_NEW_SESSION_HIGH] = newSessionId >> 8;
	packet->buf[ATEM_INDEX_NEW_SESSION_LOW] = newSessionId;
	sendPacket(packet);
}

// Creates and sends an ATEM close request packet
void sendClosePacket(uint16_t sessionId, struct sockaddr sockAddr, socklen_t sockLen) {
	packet_t* packet = createPacket(ATEM_LEN_SYN, sessionId, sockAddr, sockLen);
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSING;
	packet->resendsLeft = 1;
	sendPacket(packet);

}

// Creates an ATEM data packet for a session
packet_t* createDataPacket(uint16_t len, session_t* session) {
	// Adds length of ATEM header
	len += ATEM_LEN_HEADER;

	// Creates packet for session
	packet_t* packet = createPacket(len, session->id, session->sockAddr, session->sockLen);

	// Sets flag to request acknowledgement for packet and remote id to use with ack
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST | len >> 8;
	session->remote = (session->remote + 1) % 0x7fff;
	packet->buf[ATEM_INDEX_REMOTEID_HIGH] = session->remote >> 8;
	packet->buf[ATEM_INDEX_REMOTEID_LOW] = session->remote & 0xff;

	return packet;
}

// Creates and send a chunk of commands as an ATEM packet
void broadcastAtemCommands(const uint8_t* commands, const uint16_t len) {
	session_t* session = sessionsHead;
	while (session != NULL) {
		packet_t* packet = createDataPacket(len, session);
		memcpy(packet->buf + ATEM_LEN_HEADER, commands, len);
		sendPacket(packet);
		session = session->next;
	}
}

// Dumps all ATEM data on a newly connected client
void dumpAtemData(struct session_t* session) {
	// Sends version command
	packet_t* packet = createDataPacket(ATEM_LEN_VER, session);
	packet->buf[ATEM_INDEX_CMDLEN] = ATEM_LEN_VER;
	memcpy(packet->buf + ATEM_LEN_HEADER + 4, "_ver", 4);
	packet->buf[ATEM_INDEX_VERMAJ] = 2;
	packet->buf[ATEM_INDEX_VERMIN] = 30;
	sendPacket(packet);

	// Resets ATEM connection to get all data from switcher
	restartAtemConnection();
}

// Creates a new session and dumps all ATEM data on new connection
void addNewSession(packet_t* packet, struct sockaddr sockAddr, socklen_t sockLen) {
	// Creates a new session
	session_t* session = (session_t*)malloc(sizeof(session_t));

	// Throw on malloc fail
	if (session == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}

	// Adds the session to the linked list
	if (sessionsHead == NULL) {
		sessionsHead = session;
	}
	else {
		sessionsTail->next = session;
	}
	session->next = NULL;
	session->previous = sessionsTail;
	sessionsTail = session;

	// Sets session values
	session->id = packet->buf[ATEM_INDEX_NEW_SESSION_HIGH] << 8 |
		packet->buf[ATEM_INDEX_NEW_SESSION_LOW] | 0x8000;
	session->remote = 0;
	session->sockAddr = sockAddr;
	session->sockLen = sockLen;

	// Dumps all ATEM data on new client
	dumpAtemData(session);

	// Prints log when a new session is created
	printf("Session created %.4u\n", session->id & ~0x8000);
}

// Removes a proxy connection from the list
void removeSession(uint16_t sessionId) {
	// Gets the session
	session_t* session = sessionsHead;
	while (session != NULL && session->id != sessionId) {
		session = session->next;
	}
	if (session == NULL) return;

	// Removes session from the session list
	if (sessionsHead == session) {
		sessionsHead = session->next;
	}
	else {
		session->previous->next = session->next;
	}
	if (sessionsTail == session) {
		sessionsTail = session->previous;
	}
	else {
		session->next->previous = session->previous;
	}

	// Removes packets from queue connected to this session
	packet_t* packet = queueHead;
	while (packet != NULL) {
		if ((packet->buf[ATEM_INDEX_SESSION_HIGH] << 8 | packet->buf[ATEM_INDEX_SESSION_LOW]) == sessionId) {
			dequeuePacket(packet);
			packet_t* oldPacket = packet;
			packet = packet->next;
			free(oldPacket);
		}
		else {
			packet = packet->next;
		}
	}

	// Frees the session memory
	free(session);
}



// Processes ATEM packet from client to proxy server
void processServerData() {
	// Reads incoming data
	uint8_t buf[ATEM_MAX_PACKET_LEN];
	struct sockaddr sockAddr;
	socklen_t sockLen = sizeof(sockAddr);
	if (recvfrom(proxySock, buf, ATEM_MAX_PACKET_LEN, 0, &sockAddr, &sockLen) == -1) {
		perror("Unable to read data from relay socket");
		return;
	}

	// Gets session id for received packet
	const uint16_t sessionId = buf[ATEM_INDEX_SESSION_HIGH] << 8 | buf[ATEM_INDEX_SESSION_LOW];

	// Processes connection changes
	if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN) {
		// Sends synack as response to syn packets
		if (buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_OPEN) {
			if (findPacketInQueue(buf) != NULL) return;
			sendConnectPacket(sessionId, sockAddr, sockLen);
		}
		// Closes session on request
		else if (buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSING) {
			// Removes session from list
			removeSession(sessionId);

			// Prints log why session was disconnected
			printf("Session closed by client %.4u\n", sessionId & ~0x8000);

			// Sends close response that is not to be responded to
			uint8_t res[ATEM_LEN_SYN] = {
				[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
				[ATEM_INDEX_LEN] = ATEM_LEN_SYN,
				[ATEM_INDEX_SESSION_HIGH] = buf[ATEM_INDEX_SESSION_HIGH],
				[ATEM_INDEX_SESSION_LOW] = buf[ATEM_INDEX_SESSION_LOW],
				[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSED,
			};
			if (sendto(proxySock, res, ATEM_LEN_SYN, 0, &sockAddr, sockLen) == -1) {
				perror("Unable to send close packet to relay socket");
			}
		}
	}
	// Processes acks
	else if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACK) {
		// Removes acknowledged packet from packet queue
		packet_t* packet = findPacketInQueue(buf);
		if (packet == NULL) return;
		dequeuePacket(packet);

		// Completes handshake if it is an ack to a synack
		if (!(buf[ATEM_INDEX_SESSION_HIGH] & 0x80)) {
			addNewSession(packet, sockAddr, sockLen);
		}

		// Frees memory for acknowledged packet
		free(packet);
	}
	// Responds to ack request
	else if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACKREQUEST) {
		// Ignores packets to old server instances
		if (sessionId > lastSessionId) return;

		// Sends acknowledgement to packet requiring acknowledgement
		uint8_t res[ATEM_LEN_HEADER] = {
			[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACK,
			[ATEM_INDEX_LEN] = ATEM_LEN_HEADER,
			[ATEM_INDEX_SESSION_HIGH] = buf[ATEM_INDEX_SESSION_HIGH],
			[ATEM_INDEX_SESSION_LOW] = buf[ATEM_INDEX_SESSION_LOW],
			[ATEM_INDEX_ACKID_HIGH] = buf[ATEM_INDEX_REMOTEID_HIGH],
			[ATEM_INDEX_ACKID_LOW] = buf[ATEM_INDEX_REMOTEID_LOW]
		};
		if (sendto(proxySock, res, ATEM_LEN_HEADER, 0, &sockAddr, sockLen)) {
			perror("Unable to send acknowledgement to relay socket");
		}
	}
}

// Resends packets that has not been acknowledged
void resendServerPacket() {
	// Resends packet if there are still resends available for the packet
	if (queueHead->resendsLeft) {
		queueHead->resendsLeft--;
		packet_t* packet = queueHead;
		dequeuePacket(packet);
		sendPacket(packet);
	}
	// Removes close packets that are not responded to, they do not have a session anymore
	else if (
		queueHead->buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN &&
		queueHead->buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSING
	) {
		queueHead = queueHead->next;
	}
	// Removes the session if the packet has retried to many times
	else {
		// Creates and sends close request
		uint16_t sessionId = queueHead->buf[ATEM_INDEX_SESSION_HIGH] << 8 |
			queueHead->buf[ATEM_INDEX_SESSION_LOW];
		sendClosePacket(sessionId, queueHead->sockAddr, queueHead->sockLen);

		// Removes the session from the proxy server
		queueHead = queueHead->next;
		removeSession(sessionId);

		// Prints log why session was disconnected
		printf("Session closed by server %.4u\n", sessionId & ~0x8000);
	}
}

// Sends ping to all connected clients
void pingServerClients() {
	session_t* session = sessionsHead;
	while (session != NULL) {
		sendPacket(createDataPacket(0, session));
		session = session->next;
	}
}



// Time of last batch of ping packets
struct timeval lastProxyPing;

// Maintains proxy client connections
void maintainProxyConnections(const bool socketHasData) {
	// Processes proxy data if available
	if (socketHasData) {
		processServerData();
	}

	// Resends packets that needs to be resent
	while (queueHead != NULL && getTimeDiff(queueHead->timestamp) > ATEM_RESEND_TIME) {
		resendServerPacket();
	}

	// Pings proxy sockets on an interval
	if (getTimeDiff(lastProxyPing) > ATEM_PING_INTERVAL) {
		pingServerClients();

		// Updates timestamp for last ping cycle to current time
		getTime(&lastProxyPing);
	}
}



// Event loop to process all incoming data over the network
void loop() {
	// Calculates the remaining time of the timeout for oldest session
	struct timeval timeout = { 0 };
	uint16_t remainingTime = (queueHead != NULL) ? getTimeDiff(queueHead->timestamp) : 0;
	remainingTime = (remainingTime > ATEM_RESEND_TIME) ? ATEM_RESEND_TIME : remainingTime;
	timeout.tv_usec = (ATEM_RESEND_TIME - remainingTime) * 1000;

	// Awaits next socket event
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(atemSock, &fds);
	FD_SET(proxySock, &fds);
	const int nfds = (proxySock > atemSock) ? proxySock : atemSock;
	if (select(nfds + 1, &fds, NULL, NULL, &timeout) == -1) {
		perror("Select got an error");
		exit(EXIT_FAILURE);
	}

	// Processes available data and does maintenance
	maintainProxyConnections(FD_ISSET(proxySock, &fds));
	maintainAtemConnection(FD_ISSET(atemSock, &fds));
}

// Entry point for software
int main(int argc, char** argv) {
	// Prints usage text if no arguments were available
	if (argc < 2) {
		printf("Usage: %s <atem addr>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	setupAtemSocket(argv[1]);
	setupProxyServer();
	while (true) loop();
	return 0;
}
