#ifndef HELPERS_H
#define HELPERS_H

#include <stdbool.h> // bool, true, false
#include <stdint.h> // uint8_t
#include <stdio.h> // printf, perror
#include <stdlib.h> // size_t
#include <time.h> // timespec

#include "../src/atem.h"

#define ATEM_MASK_LEN_HIGH (ATEM_MAX_PACKET_LEN >> 8)

#define ATEM_INDEX_FLAGS 0
#define ATEM_INDEX_LEN_HIGH 0
#define ATEM_INDEX_LEN_LOW 1
#define ATEM_INDEX_SESSION_HIGH 2
#define ATEM_INDEX_SESSION_LOW 3
#define ATEM_INDEX_ACKID_HIGH 4
#define ATEM_INDEX_ACKID_LOW 5
#define ATEM_INDEX_LOCALID_HIGH 6
#define ATEM_INDEX_LOCALID_LOW 7
#define ATEM_INDEX_REMOTEID_HIGH 10
#define ATEM_INDEX_REMOTEID_LOW 11
#define ATEM_INDEX_OPCODE 12
#define ATEM_INDEX_NEW_SESSION_HIGH 14
#define ATEM_INDEX_NEW_SESSION_LOW 15

#define ATEM_LEN_HEADER 12
#define ATEM_LEN_SYN 20
#define ATEM_LEN_CMDHEADER 8

#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

#define ATEM_CONNECTION_SUCCESS 0x02
#define ATEM_CONNECTION_OPENING 0x01

#define ATEM_HANDSHAKE_RESENDS 10



#define TIMER_SPAN (0.8)



struct test_t {
	void (*fn)(void);
	char* name;
};

#define GET_TEST(value) { .fn = value, .name = #value }



#define PRINT_READ_FLAG (1)
#define PRINT_WRITE_FLAG (2)

void setPrintFlags(int flags);
void clearPrintFlags(int flags);
void setMaxLenPrint(int maxLen);
void printBuffer(uint8_t* buf, int len);

extern int errorsEncountered;
void runTest(struct test_t* test, bool continueOnAbort);
void abortCurrentTest();

int bufferGetIdFromIndex(uint8_t* buf, int high, int low);
void bufferByteIsZero(uint8_t* buf, int index);
int bufferGetFlags(uint8_t* buf);
void bufferHasFlags(uint8_t* buf, int requiredFlags, int optionalFlags);
int bufferGetLen(uint8_t* buf);
void bufferHasLength(uint8_t* buf, int expectLen);
int bufferGetSessionId(uint8_t* buf);
void bufferHasSessionId(uint8_t* buf, int expectSessionId);
int bufferGetAckId(uint8_t* buf);
void bufferHasAckId(uint8_t* buf, int expectAckId);
int bufferGetLocalId(uint8_t* buf);
void bufferHasLocalId(uint8_t* buf, int expectLocalId);
int bufferGetRemoteId(uint8_t* buf);
void bufferHasRemoteId(uint8_t* buf, int expectRemoteId);
int bufferGetOpcode(uint8_t* buf);
void bufferHasOpcode(uint8_t* buf, int expectOpcode);
int bufferGetHandshakeSessionId(uint8_t* buf);
void bufferHasHandshakeSessionId(uint8_t* buf, int expectSessionId);
bool bufferGetResend(uint8_t* buf);
void bufferHasResend(uint8_t* buf, bool expectResendFlag);

extern int sock;
void setupSocket(char* addr);
void atem_read(struct atem_t* atem);
void atem_write(struct atem_t* atem);
void expectNoData();

int handshakeWrite(struct atem_t* atem);
void validateOpcode(struct atem_t* atem, int sessionId, bool isResend);
void readAndValidateOpcode(struct atem_t* atem, int sessionId, bool isResend, int expectOpcode);
int handshakeReadSuccess(struct atem_t* atem, int sessionId, bool isResend);
int connectHandshake(struct atem_t* atem);

void timerSet(struct timespec* ts);
size_t timerGetDiff(struct timespec* ts);
void timerHasDiff(struct timespec* ts, int expectedDiff);

#endif
