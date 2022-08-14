#include "./helpers.h"



static int readAndGetOpcode(struct atem_t* atem) {
	atem_read(atem);
	return bufferGetOpcode(atem->readBuf);
}

// Connects to ATEM, refuse to respond to SYNACK until ATEM requests closing
static int timeoutHandshake(struct atem_t* atem) {
	int clientSessionId = handshakeWrite(atem);
	int serverSessionId = handshakeReadSuccess(atem, clientSessionId, false);

	int resendsBeforeClose = 0;
	while (readAndGetOpcode(atem) == ATEM_CONNECTION_SUCCESS) {
		validateOpcode(atem, clientSessionId, true);
		bufferHasHandshakeSessionId(atem->readBuf, serverSessionId);
		resendsBeforeClose++;
	}
	if (resendsBeforeClose != ATEM_HANDSHAKE_RESENDS) {
		printf("Expected %d resends, but got %d\n", ATEM_HANDSHAKE_RESENDS, resendsBeforeClose);
	}

	validateOpcode(atem, serverSessionId | 0x8000, false);
	bufferHasHandshakeSessionId(atem->readBuf, 0x0000);

	return serverSessionId | 0x8000;
}

static void sendClose(struct atem_t* atem, int serverSessionId) {
	uint8_t buf[ATEM_LEN_SYN] = {
		[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
		[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
		[ATEM_INDEX_SESSION_HIGH] = serverSessionId >> 8,
		[ATEM_INDEX_SESSION_LOW] = serverSessionId & 0xff,
		[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSED
	};
	atem->writeBuf = buf;
	atem->writeLen = ATEM_LEN_SYN;
	atem_write(atem);
}

static void requestCloseBeforeConnected(struct atem_t* atem, int serverSessionId) {
	atem_connection_close(atem);
	atem->writeBuf[ATEM_INDEX_SESSION_HIGH] = serverSessionId >> 8;
	atem->writeBuf[ATEM_INDEX_SESSION_LOW] = serverSessionId & 0xff;
	atem_write(atem);
}

static void awaitOpcode(struct atem_t* atem, int expectSessionId, int opcode) {
	do {
		atem_read(atem);
	} while (!(bufferGetFlags(atem->readBuf) & ATEM_FLAG_SYN));
	bufferHasOpcode(atem->readBuf, opcode);
	validateOpcode(atem, expectSessionId, false);
}

static void flushUntilVersion(struct atem_t* atem) {
 	do {
		atem_read(atem);
		atem_parse(atem);
		atem_write(atem);
		while (atem_cmd_available(atem)) switch (atem_cmd_next(atem)) {
			case ATEM_CMDNAME_VERSION: return;
		}
	} while (bufferGetRemoteId(atem->readBuf) < 10);

	fprintf(stderr, "Did not receive version command as expected\n");
	abortCurrentTest();
}

static int connectAndDisconnect(struct atem_t* atem) {
	int serverSessionId = connectHandshake(atem) | 0x8000;
	flushUntilVersion(atem);

	atem_connection_close(atem);
	atem_write(atem);
	awaitOpcode(atem, serverSessionId, ATEM_CONNECTION_CLOSED);

	return serverSessionId;
}

static int connectAndDrop(struct atem_t* atem) {
	int serverSessionId = connectHandshake(atem) | 0x8000;
	flushUntilVersion(atem);
	awaitOpcode(atem, serverSessionId, ATEM_CONNECTION_CLOSING);
	return serverSessionId;
}



// Tests connecting to ATEM without responding to SYNACK or closing
void handshake_test1() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);
	readAndValidateOpcode(&atem, serverSessionId, true, ATEM_CONNECTION_CLOSING);
	bufferHasHandshakeSessionId(atem.readBuf, 0x0000);
}

// Tests connecting to ATEM without responding to SYNACK but responds to closing
void handshake_test2() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);
	sendClose(&atem, serverSessionId);
}

// Tests requesting close before handshake is complete
void handshake_test3() {
	struct atem_t atem;

	int clientSessionId = handshakeWrite(&atem);
	readAndValidateOpcode(&atem, clientSessionId, false, ATEM_CONNECTION_SUCCESS);
	int serverSessionId = bufferGetHandshakeSessionId(atem.readBuf) | 0x8000;
	readAndValidateOpcode(&atem, clientSessionId, true, ATEM_CONNECTION_SUCCESS);
	readAndValidateOpcode(&atem, clientSessionId, true, ATEM_CONNECTION_SUCCESS);

	requestCloseBeforeConnected(&atem, serverSessionId);
	readAndValidateOpcode(&atem, serverSessionId, false, ATEM_CONNECTION_CLOSED);
}

// Test requesting close after receiving closing
void handshake_test4() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);

	requestCloseBeforeConnected(&atem, serverSessionId);

	readAndValidateOpcode(&atem, serverSessionId, false, ATEM_CONNECTION_CLOSED);
}

// Test closing connection after handshake
void handshake_test5() {
	struct atem_t atem;
	connectAndDisconnect(&atem);
}

// Test dropping connection after handshake, not responding to closing
void handshake_test6() {
	struct atem_t atem;
	int serverSessionId = connectAndDrop(&atem);
	readAndValidateOpcode(&atem, serverSessionId, true, ATEM_CONNECTION_CLOSING);
	bufferHasHandshakeSessionId(atem.readBuf, 0x0000);
}

// Test dropping connection after handshake, responding to closing
void handshake_test7() {
	struct atem_t atem;
	int serverSessionId = connectAndDrop(&atem);
	sendClose(&atem, serverSessionId);
}

