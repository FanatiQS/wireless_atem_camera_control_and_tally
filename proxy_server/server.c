#include <stdio.h> // perror, fprintf, stderr, printf
#include <stdint.h> // uint8_t uint16_t
#include <stdbool.h> // true
#include <string.h> // memset, memcpy
#include <stdlib.h> // malloc, free, NULL, exit, EXIT_FAILURE

#ifdef _WIN32 // Windows specific includes
#include <winsock2.h> // AF_INET, SOCK_DGRAM, socket, connect, bind, send, sendto, recv, recvfrom, sockaddr, sockaddr_in, INADDR_ANY, inet_addr, htons, fd_set, FD_ZERO, FD_SET, FD_ISSET, select, timeval, SOCKET, INVALID_SOCKET
typedef unsigned long in_addr_t;
typedef int socklen_t;
// @todo Missing function gettimeofday
#else // Unix specific includes
#include <sys/socket.h> // AF_INET, SOCK_DGRAM, socket, connect, bind, send, sendto, recv, recvfrom, socklen_t, sockaddr
#include <netinet/in.h> // in_addr_t, sockaddr_in, INADDR_ANY
#include <arpa/inet.h> // inet_addr, htons
#include <sys/select.h> // fd_set, FD_ZERO, FD_SET, FD_ISSET, select
#include <sys/time.h> // timeval, gettimeofday
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#endif

#include "../src/atem.h"



//!!
#define ATEM_LEN_SYN 20
#define ATEM_LEN_ACK 12
#define ATEM_LEN_HEADER 12
#define ATEM_LEN_VER 24

//!!
#define ATEM_INDEX_FLAGS 0
#define ATEM_INDEX_LEN 1
#define ATEM_INDEX_SESSION 2
#define ATEM_INDEX_ACKID 4
#define ATEM_INDEX_REMOTEID 10
#define ATEM_INDEX_OPCODE 12
#define ATEM_INDEX_CMDLEN (ATEM_LEN_HEADER + 1)
#define ATEM_INDEX_VERMAJ (ATEM_LEN_HEADER + 9)
#define ATEM_INDEX_VERMIN (ATEM_INDEX_VERMAJ + 2)

//!!
#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

//!!
#define ATEM_CONNECTION_OPEN 0x01
#define ATEM_CONNECTION_SUCCESS 0x02

//!!
#define ATEM_PING_INTERVAL 2000
#define ATEM_RESEND_TIME 100



// Socket for communicating with ATEM switcher and proxy connections
SOCKET atemSock;
SOCKET proxySock;

// State for ATEM connection
struct atem_t atem;
struct timeval atemLastContact;

// Creates a socket
SOCKET createSocket() {
	const SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}
	return sock;
}

// Creates a socket address struct with address and port
struct sockaddr_in createAddr(const in_addr_t addr) {
	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(ATEM_PORT);
	sockaddr.sin_addr.s_addr = addr;
	return sockaddr;
}

// Sends the buffered ATEM data to the ATEM switcher
void sendAtem() {
	send(atemSock, atem.writeBuf, atem.writeLen, 0);
}

// Initializes ATEM socket, proxy socket and ATEM states
void initServer(const char* addr) {
	// Creates a socket connection to the ATEM switcher
 	atemSock = createSocket();
	const struct sockaddr_in atemSockAddr = createAddr(inet_addr(addr));
	if (atemSockAddr.sin_addr.s_addr == -1) {
		fprintf(stderr, "Invalid address\n");
		exit(EXIT_FAILURE);
	}
	if (connect(atemSock, (const struct sockaddr *)&atemSockAddr, sizeof(atemSockAddr))) {
		perror("Failed to connect socket");
		exit(EXIT_FAILURE);
	}

	// Creates a proxy socket to connect to
 	proxySock = createSocket();
	const struct sockaddr_in proxySockAddr = createAddr(INADDR_ANY);
	if (bind(proxySock, (const struct sockaddr *)&proxySockAddr, sizeof(proxySockAddr))) {
		perror("Failed to bind socket");
		exit(EXIT_FAILURE);
	}

	// Initializes ATEM connection to switcher
	gettimeofday(&atemLastContact, NULL);
	resetAtemState(&atem);
	sendAtem();
}



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

// Queue of packets awaiting acknowledgement
packet_t* queueHead;
packet_t* queueTail;

// Creates an ATEM packet for proxy a proxy connection
packet_t* createPacket(uint16_t bufLen, uint16_t sessionId, struct sockaddr sockAddr, socklen_t sockLen) {
	// Creates a packet for keeping information about the response
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

	// Sets length and session id of response buffer
	memset(packet->buf, 0, bufLen);
	packet->bufLen = bufLen;
	packet->buf[ATEM_INDEX_LEN] = bufLen;
	packet->buf[ATEM_INDEX_SESSION] = sessionId >> 8;
	packet->buf[ATEM_INDEX_SESSION + 1] = sessionId & 0xff;

	// Returns packet
	return packet;
}

// Sends a packet to a proxy connection and adds it to the queue
void sendPacket(packet_t* packet) {
	// Sets resend flag if this is not the first time the packet has been transmitted
	if (packet->timestamp.tv_sec != 0) packet->buf[0] |= ATEM_FLAG_RETRANSMIT;

	// Sends buffer of packet
	sendto(proxySock, packet->buf, packet->bufLen, 0, &packet->sockAddr, packet->sockLen);

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
	gettimeofday(&packet->timestamp, NULL);
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

// Finds a packet in the queue matching the buffer
packet_t* findPacketInQueue(uint8_t* buf) {
	packet_t* packet = queueHead;
	while (packet != NULL) {
		if (
			packet->buf[ATEM_INDEX_SESSION] == buf[ATEM_INDEX_SESSION] &&
			packet->buf[ATEM_INDEX_SESSION + 1] == buf[ATEM_INDEX_SESSION + 1] &&
			packet->buf[ATEM_INDEX_ACKID] == buf[ATEM_INDEX_REMOTEID] &&
			packet->buf[ATEM_INDEX_ACKID + 1] == buf[ATEM_INDEX_REMOTEID + 1]
		) {
			break;
		}
		packet = packet->next;
	}
	return packet;
}



// Structure for proxy connections
uint16_t lastSessionId = 0;
typedef struct session_t {
	struct session_t* next;
	struct session_t* previous;
	uint16_t id;
	uint16_t remote;
	struct sockaddr sockAddr;
	socklen_t sockLen;
} session_t;

// List of all proxy connections
session_t* sessionsHead;
session_t* sessionsTail;

// Creates a new session and dumps all ATEM data on new connection
void addNewSession(struct sockaddr sockAddr, socklen_t sockLen) {
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
	session->id = (++lastSessionId % 0x7fff) | 0x8000;
	session->remote = 1;
	session->sockAddr = sockAddr;
	session->sockLen = sockLen;

	// Sends version command
	packet_t* packet = createPacket(ATEM_LEN_VER, session->id, session->sockAddr, session->sockLen);
	packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST;
	packet->buf[ATEM_INDEX_REMOTEID + 1] = 1;
	packet->buf[ATEM_INDEX_CMDLEN] = ATEM_LEN_VER - ATEM_LEN_HEADER;
	memcpy(packet->buf + ATEM_LEN_HEADER + 4, "_ver", 4);
	packet->buf[ATEM_INDEX_VERMAJ] = 2;
	packet->buf[ATEM_INDEX_VERMIN] = 30;
	sendPacket(packet);

	// Resets ATEM connection to get all data from switcher
	closeAtemConnection(&atem);
	sendAtem();
}

// Removes a proxy connection from the list
void removeSession(uint16_t sessionId) {
	// Gets the session
	session_t* session = sessionsHead;
	while (session != NULL && session->id != sessionId) {
		session = session->next;
	}
	if (session == NULL) return;

	// Removes the session from the session list
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
		if (
			packet->buf[ATEM_INDEX_SESSION] == (sessionId >> 8) &&
			packet->buf[ATEM_INDEX_SESSION + 1] == (sessionId & 0xff)
		) {
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



// Processes incoming data from ATEM
void processAtemData() {
	// Reads data from the ATEM switcher
	if (recv(atemSock, atem.readBuf, ATEM_MAX_PACKET_LEN, 0) == -1) {
		closeAtemConnection(&atem);
		sendAtem();
		return;
	}

	// Parses read buffer from ATEM
	uint8_t* writeBuf = atem.readBuf + ATEM_LEN_HEADER;
	parseAtemData(&atem);
	while (hasAtemCommand(&atem)) {
		switch (nextAtemCommand(&atem)) {
			default: continue;
			case ATEM_CMDNAME_VERSION: {
				printf("connected atem\n");
				// @todo buffer data to send to future proxy connections
				continue;
			}
			case ATEM_CMDNAME_CAMERACONTROL: {
				// @todo buffer data to send to future proxy connections
				break;
			}
			case ATEM_CMDNAME_TALLY: {
				// @todo buffer data to send to future proxy connections
				break;
			}
		}

		// Filters out interesting command for proxy sockets
		memcpy(writeBuf, atem.cmdBuf - 8, atem.cmdLen + 8);
		writeBuf += atem.cmdLen;
	}

	// Gets length of packet after filtering out unisteresting commands
	uint16_t writeLen = writeBuf - atem.readBuf;

	// Sends response to atem switcher
	sendAtem();

	// Updates time stamp for most recent atem packet
	gettimeofday(&atemLastContact, NULL);

	// Retransmits the filtered data to proxy connections
	if (writeLen > ATEM_LEN_HEADER) {
		session_t* session = sessionsHead;
		while (session != NULL) {
			packet_t* packet = createPacket(writeLen, session->id, session->sockAddr, session->sockLen);
			packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST | writeLen >> 8;
			memcpy(packet->buf + 4, atem.readBuf + 4, writeLen - 4);
			sendPacket(packet);
			session = session->next;
		}
	}
}

// Processes incoming data from a proxy connection
void processRelayData() {
	// Reads incoming data
	uint8_t buf[ATEM_MAX_PACKET_LEN];
	struct sockaddr sockAddr;
	socklen_t sockLen = sizeof(sockAddr);
	if (recvfrom(proxySock, buf, ATEM_MAX_PACKET_LEN, 0, &sockAddr, &sockLen) == -1) {
		perror("Unable to read data from relay socket");
		return;
	}

	// Processes connection changes
	if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN) {
		// Gets session id for received packet
		const uint16_t sessionId = buf[ATEM_INDEX_SESSION] << 8 | buf[ATEM_INDEX_SESSION + 1];

		// Sends synack as response to syn packets
		if (buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_OPEN) {
			if (findPacketInQueue(buf) != NULL) return;
			packet_t* packet = createPacket(ATEM_LEN_SYN, sessionId, sockAddr, sockLen);
			packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
			packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_SUCCESS;
			sendPacket(packet);
		}
		// Closes session on request
		else if (buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSING) {
			// Removes session from list
			removeSession(sessionId);

			// Sends close response
			uint8_t res[ATEM_LEN_SYN] = { ATEM_FLAG_SYN };
			res[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
			res[ATEM_INDEX_LEN] = ATEM_LEN_SYN;
			res[ATEM_INDEX_SESSION] = sessionId >> 8;
			res[ATEM_INDEX_SESSION + 1] = sessionId & 0xff;
			res[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSED;
			if (sendto(proxySock, res, ATEM_LEN_SYN, 0, &sockAddr, sockLen) == -1) {
				perror("Unable to send data to relay socket");
			}
		}
	}
	// Processes acks
	else if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACK) {
		// Removes acknowledged packet from packet queue
		packet_t* packet = findPacketInQueue(buf);
		if (packet == NULL) return;
		dequeuePacket(packet);
		free(packet);

		// Completes handshake
		if (buf[ATEM_INDEX_ACKID] == 0x00 && buf[ATEM_INDEX_ACKID + 1] == 0x00) {
			addNewSession(sockAddr, sockLen);
		}
	}
}



// Gets the time difference in milliseconds between previous timestamp and now
uint16_t getTimeDiff(struct timeval timestamp) {
	// Gets current time
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);

	// Gets millisecond precision difference in time between now and timestamp
	uint16_t diff = 0;
	diff += (currentTime.tv_sec - timestamp.tv_sec) * 1000;
	diff += (currentTime.tv_usec - timestamp.tv_usec) / 1000;
	return diff;
}

// Resends packets that has not been acknowledged
void processResends() {
	while (queueHead != NULL && getTimeDiff(queueHead->timestamp) > ATEM_RESEND_TIME) {

		// Resends packet if there are still resends available for the packet
		if (queueHead->resendsLeft) {
			queueHead->resendsLeft--;
			packet_t* packet = queueHead;
			dequeuePacket(queueHead);
			sendPacket(packet);
		}
		// Removes the session if the packet has retried to many times
		else if (queueHead->buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN &&
			queueHead->buf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSING
		) {
			queueHead = queueHead->next;
		}
		else {
			// Creates close request
			uint16_t sessionId = queueHead->buf[ATEM_INDEX_SESSION] << 8 |
				queueHead->buf[ATEM_INDEX_SESSION + 1];
			packet_t* packet = createPacket(ATEM_LEN_SYN, sessionId,
					queueHead->sockAddr, queueHead->sockLen);
			packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
			packet->buf[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSING;
			packet->resendsLeft = 1;

			// Removes the session and sends the close request
			queueHead = queueHead->next;
			removeSession(sessionId);
			sendPacket(packet);
		}
	}
}

// Time of last batch of ping packets
struct timeval lastPing;

// Pings all proxy sockets
void sendPings() {
	// Sends pring to all connected clients
	session_t* session = sessionsHead;
	while (session != NULL) {
		packet_t* packet = createPacket(ATEM_LEN_ACK, session->id, session->sockAddr, session->sockLen);
		packet->buf[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQUEST;
		session->remote = (session->remote + 1) % 0x7fff;
		packet->buf[ATEM_INDEX_REMOTEID] = session->remote >> 8;
		packet->buf[ATEM_INDEX_REMOTEID + 1] = session->remote;
		sendPacket(packet);
		session = session->next;
	}

	// Updates timestamp for last ping cycle
	gettimeofday(&lastPing, NULL);
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

	// Processes ATEM data if available
	if (FD_ISSET(atemSock, &fds)) {
		processAtemData();
	}
	else if (getTimeDiff(atemLastContact) > ATEM_TIMEOUT * 1000) {
		resetAtemState(&atem);
		gettimeofday(&atemLastContact, NULL);
		sendAtem();
	}

	// Processes proxy data if available
	if (FD_ISSET(proxySock, &fds)) processRelayData();

	// Resends packets that needs to be resent
	processResends();

	// Pings proxy sockets on an interval
	if (getTimeDiff(lastPing) > ATEM_PING_INTERVAL) sendPings();
}

// Entry point for software
int main(int argc, char** argv) {
	// Prints usage text if no arguments were available
	if (argc < 2) {
		printf("Usage: %s <atem addr>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	initServer(argv[1]);
	while (true) loop();
	return 0;
}
