#include <stdlib.h> // exit, EXIT_FAILURE, EXIT_SUCCESS, size_t
#include <setjmp.h> // setjmp, longjmp, jmp_buf
#include <time.h> // timespec, timespec_get

#include <sys/socket.h> // ssize, recv, send, socket, AF_INET, SOCK_DGRAM, connect
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // inet_addr, htons
#include <sys/select.h> // select, fd_set, FD_ZERO, FD_SET, timeval

#include "./helpers.h"



// Shim timespec_get for MacOS 10.14 and older
#if defined(__APPLE__) && !defined(TIME_UTC)
#define timespec_get(ts, base) clock_gettime(CLOCK_REALTIME, ts)
#define TIME_UTC 0
#warning Shimming timespec_get with clock_gettime
#endif



#ifndef EXIT_WAIT
#define EXIT_WAIT (8)
#endif



static int printFlags = 0;
static int maxLenPrint = -1;

void setPrintFlags(int flags) {
	printFlags |= flags;
}

void clearPrintFlags(int flags) {
	printFlags &= ~flags;
}

void setMaxLenPrint(int maxLen) {
	maxLenPrint = maxLen;
}

void printBuffer(uint8_t* buf, int len) {
	bool clamped = len > maxLenPrint && maxLenPrint != -1;
	if (clamped) {
		len = maxLenPrint;
	}

	for (int i = 0; i < len; i++) {
		printf("%.2x ", buf[i]);
	}
	if (clamped) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}



static bool exitOnAbort = true;
static jmp_buf abortJmp;
int errorsEncountered = 0;

void runTest(struct test_t* test, bool exitOnAbortSelector) {
	exitOnAbort = exitOnAbortSelector;
	printf("Running test: %s\n", test->name);

	if (!exitOnAbort || !setjmp(abortJmp)) {
		test->fn();
		expectNoData();
	}

	printf("Completed test: %s\n", test->name);
	exitOnAbort = true;
}

void abortCurrentTest() {
	if (exitOnAbort) {
		errorsEncountered++;
		longjmp(abortJmp, true);
	}
	else {
		exit(EXIT_FAILURE);
	}
}



// Gets the 16 bit id from high and low byte of the buffer
int bufferGetIdFromIndex(uint8_t* buf, int high, int low) {
	return buf[high] << 8 | buf[low];
}

// Ensures the byte at the index of the buffer is set to zero
void bufferByteIsZero(uint8_t* buf, int index) {
	if (buf[index] == 0x00) return;

	printf("Expected byte with index %d to have a value of 0 but it had 0x%.2x\n", index, buf[index]);
	abortCurrentTest();
}



int bufferGetFlags(uint8_t* buf) {
	return buf[ATEM_INDEX_FLAGS] & ~ATEM_MASK_LEN_HIGH;
}

void bufferHasFlags(uint8_t* buf, int requiredFlags, int optionalFlags) {
	int flags = bufferGetFlags(buf);

	if (!(flags & requiredFlags)) {
		printf("Missing required flag(s) 0x%.2x 0x%.2x\n", flags, requiredFlags);
		abortCurrentTest();
	}

	int illegalFlags = ~(requiredFlags | optionalFlags);
	if (flags & illegalFlags) {
		printf("Unexpected illegal flag(s) set 0x%.2x 0x%.2x\n", flags, illegalFlags);
		abortCurrentTest();
	}
}



int bufferGetLen(uint8_t* buf) {
	return bufferGetIdFromIndex(buf, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW) & 0x7f;
}

void bufferHasLength(uint8_t* buf, int expectLen) {
	int len = bufferGetLen(buf);
	if (len == expectLen) return;

	printf("Expected packet length %d, but got %d\n", expectLen, len);
	abortCurrentTest();
}



int bufferGetSessionId(uint8_t* buf) {
	return bufferGetIdFromIndex(buf, ATEM_INDEX_SESSION_HIGH, ATEM_INDEX_SESSION_LOW);
}

void bufferHasSessionId(uint8_t* buf, int expectSessionId) {
	int sessionId = bufferGetSessionId(buf);
	if (sessionId == expectSessionId) return;

	printf("Expected session id 0x%.4x, but got 0x%.4x\n", expectSessionId, sessionId);
	abortCurrentTest();
}



// Gets the buffers ack id
int bufferGetAckId(uint8_t* buf) {
	return bufferGetIdFromIndex(buf, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW);
}

// Ensures the buffers ack id matches the expected id
void bufferHasAckId(uint8_t* buf, int expectAckId) {
	int ackId = bufferGetAckId(buf);
	if (ackId == expectAckId) return;

	printf("Expected acknowledge id 0x%.4x, but got 0x%.4x\n", expectAckId, ackId);
	abortCurrentTest();
}



// Gets the buffers local id
int bufferGetLocalId(uint8_t* buf) {
	return bufferGetIdFromIndex(buf, ATEM_INDEX_LOCALID_HIGH, ATEM_INDEX_LOCALID_LOW);
}

// Ensures the buffers local id matches the expected id
void bufferHasLocalId(uint8_t* buf, int expectLocalId) {
	int localId = bufferGetLocalId(buf);
	if (localId == expectLocalId) return;

	printf("Expected local id 0x%.4x, but got 0x%.4x\n", expectLocalId, localId);
	abortCurrentTest();
}



int bufferGetRemoteId(uint8_t* buf) {
	return bufferGetIdFromIndex(buf, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW);
}

void bufferHasRemoteId(uint8_t* buf, int expectRemoteId) {
	int remoteId = bufferGetLocalId(buf);
	if (remoteId == expectRemoteId) return;

	printf("Expected remote id 0x%.4x, but got 0x%.4x\n", expectRemoteId, remoteId);
	abortCurrentTest();
}



// Gets the opcode from a SYN packet
int bufferGetOpcode(uint8_t* buf) {
	bufferHasFlags(buf, ATEM_FLAG_SYN, ATEM_FLAG_RETRANSMIT);
	bufferHasLength(buf, ATEM_LEN_SYN);

	bufferHasAckId(buf, 0x0000);
	bufferHasLocalId(buf, 0x0000);
	bufferHasRemoteId(buf, 0x0000);

	bufferByteIsZero(buf, 13);
	bufferByteIsZero(buf, 16);
	bufferByteIsZero(buf, 17);
	bufferByteIsZero(buf, 18);
	bufferByteIsZero(buf, 19);

	return buf[ATEM_INDEX_OPCODE];
}

// Ensures the buffer is a SYN packet with the specified opcode
void bufferHasOpcode(uint8_t* buf, int expectOpcode) {
	int opcode = bufferGetOpcode(buf);
	if (opcode == expectOpcode) return;

	printf("Expected opcode 0x%.2x, but got 0x%.2x\n", expectOpcode, opcode);
	abortCurrentTest();
}



// Gets the server assigned session id from a SYNACK packet with opcode ATEM_CONNECTION_SUCCESS
int bufferGetHandshakeSessionId(uint8_t* buf) {
	int id = bufferGetIdFromIndex(buf, ATEM_INDEX_NEW_SESSION_HIGH, ATEM_INDEX_NEW_SESSION_LOW);

	if (id & 0x8000) {
		fprintf(stderr,
			"MSB of server assigned session id from handshake was unexpectedly set (0x%.4x)\n", id
		);
		abortCurrentTest();
	}

	return id;
}

void bufferHasHandshakeSessionId(uint8_t* buf, int expectSessionId) {
	int sessionId = bufferGetHandshakeSessionId(buf);
	if (sessionId == expectSessionId) return;

	printf("Expected handshake session id 0x%.4x, but got 0x%.4x\n", expectSessionId, sessionId);
	abortCurrentTest();
}



bool bufferGetResend(uint8_t* buf) {
	return bufferGetFlags(buf) & ATEM_FLAG_RETRANSMIT;
}

void bufferHasResend(uint8_t* buf, bool expectResendFlag) {
	bool resendFlag = bufferGetResend(buf);
	if (resendFlag == expectResendFlag) return;

	printf("Expected packet resend status to be %d, but it was %d\n", expectResendFlag, resendFlag);
	abortCurrentTest();
}



// Socket used for all ATEM communication
int sock;

void setupSocket(char* addr) {
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	if (sock == -1) {
		perror("Failed to create socket");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr = { .s_addr = inet_addr(addr) }
	};

	if (connect(sock, (const struct sockaddr*)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect socket to ATEM address");
		exit(EXIT_FAILURE);
	}

	struct timeval tv = { .tv_sec = 10 };
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
		perror("Failed to set timeout for socket");
		exit(EXIT_FAILURE);
	}
}

void atem_write(struct atem_t* atem) {
	ssize_t sendLen = send(sock, atem->writeBuf, atem->writeLen, 0);

	if (sendLen == -1) {
		perror("Faild to write data");
		abortCurrentTest();
	}

	if (printFlags & PRINT_WRITE_FLAG) {
		printf("Sent packet: ");
		printBuffer(atem->writeBuf, atem->writeLen);
	}
}

void atem_read(struct atem_t* atem) {
	ssize_t recvLen = recv(sock, atem->readBuf, ATEM_MAX_PACKET_LEN, 0);

	if (recvLen == -1) {
		perror("Failed to read data");
		abortCurrentTest();
	}

	if (recvLen < ATEM_LEN_HEADER) {
		printf("UDP packet was too short to include an ATEM header\n");
		abortCurrentTest();
	}

	int packetLen = bufferGetLen(atem->readBuf);
	if (packetLen != recvLen) {
		fprintf(stderr,
			"Received packet length did not match protocol defined length %d %zu\n",
			packetLen, recvLen
		);
		abortCurrentTest();
	}

	if (printFlags & PRINT_READ_FLAG) {
		printf("Received packet: ");
		printBuffer(atem->readBuf, recvLen);
	}
}

void expectNoData() {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	struct timeval tv = { .tv_sec = EXIT_WAIT };

	int selectLen = select(sock + 1, &fds, NULL, NULL, &tv);

	if (selectLen == -1) {
		perror("Select got an error");
		exit(EXIT_FAILURE);
	}

	if (selectLen == 0) return;

	printf("Received data when expecting not to\n");
	abortCurrentTest();
}



int handshakeWrite(struct atem_t* atem) {
	atem_connection_reset(atem);
	atem_write(atem);
	int sessionId = bufferGetIdFromIndex(atem->writeBuf, ATEM_INDEX_SESSION_HIGH, ATEM_INDEX_SESSION_LOW);

	if (sessionId & 0x8000) {
		fprintf(stderr, "MSB if client assigned session id was unexpectedly set (0x%.4x)\n", sessionId);
		abortCurrentTest();
	}

	return sessionId;
}

void validateOpcode(struct atem_t* atem, int sessionId, bool isResend) {
	bufferHasResend(atem->readBuf, isResend);
	bufferHasSessionId(atem->readBuf, sessionId);
}

void readAndValidateOpcode(struct atem_t* atem, int sessionId, bool isResend, int expectOpcode) {
	atem_read(atem);
	bufferHasOpcode(atem->readBuf, expectOpcode);
	validateOpcode(atem, sessionId, isResend);
}

int handshakeReadSuccess(struct atem_t* atem, int sessionId, bool isResend) {
	readAndValidateOpcode(atem, sessionId, isResend, ATEM_CONNECTION_SUCCESS);
	return bufferGetHandshakeSessionId(atem->readBuf);
}

int connectHandshake(struct atem_t* atem) {
	int clientSessionId = handshakeWrite(atem);
	int serverSessionId = handshakeReadSuccess(atem, clientSessionId, false);
	atem_parse(atem);
	atem_write(atem);
	return serverSessionId;
}



void timerSet(struct timespec* ts) {
	timespec_get(ts, TIME_UTC);
}

size_t timerGetDiff(struct timespec* ts) {
	struct timespec now;
	timespec_get(&now, TIME_UTC);
	return (now.tv_sec - ts->tv_sec) * 1000 + (now.tv_nsec / 1000000) - (ts->tv_nsec / 1000000);
}

void timerHasDiff(struct timespec* ts, int expectedDiff) {
	size_t diff = timerGetDiff(ts);

	if (
		diff > (int)(expectedDiff * (2 - TIMER_SPAN)) ||
		diff < (int)(expectedDiff * TIMER_SPAN)
	) {
		printf("Expected timer diff to be %dms but got %ldms\n", expectedDiff, diff);
		abortCurrentTest();
	}
}