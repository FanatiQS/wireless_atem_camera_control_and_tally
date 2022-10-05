#include "./helpers.h"



static void awaitOpcode(struct atem_t* atem, int expectSessionId, int opcode) {
	do {
		atem_read(atem);
	} while (!(bufferGetFlags(atem->readBuf) & ATEM_FLAG_SYN));
	bufferHasOpcode(atem->readBuf, opcode);
	validateOpcode(atem, expectSessionId, false);
}

static int connectAndDisconnect(struct atem_t* atem) {
	int serverSessionId = handshakeConnect(atem);
	flushStateDump(atem);

	atem_connection_close(atem);
	atem_write(atem);
	awaitOpcode(atem, serverSessionId, ATEM_CONNECTION_CLOSED);

	return serverSessionId;
}



// Tests connecting to ATEM without responding to SYNACK or closing
void handshake_test1() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);
	handshakeReadClosing(&atem, serverSessionId, true);
}

// Tests connecting to ATEM without responding to SYNACK but responds to closing
void handshake_test2() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);
	handshakeSendClosed(&atem, serverSessionId);
}

// Tests requesting close before handshake is complete
void handshake_test3() {
	struct atem_t atem;
	int serverSessionId = handshakeOpen(&atem);
	handshakeClose(&atem, serverSessionId);
}

// Test requesting close after receiving closing
void handshake_test4() {
	struct atem_t atem;
	int serverSessionId = timeoutHandshake(&atem);
	handshakeClose(&atem, serverSessionId);
}

// Test closing connection after handshake
void handshake_test5() {
	struct atem_t atem;
	int serverSessionId = handshakeConnect(&atem);
	flushStateDump(&atem);
	handshakeClose(&atem, serverSessionId);
}

// Test dropping connection after handshake, not responding to closing
void handshake_test6() {
	struct atem_t atem;
	int serverSessionId = handshakeConnect(&atem);
	flushStateDump(&atem);
	awaitOpcode(&atem, serverSessionId, ATEM_CONNECTION_CLOSING);
	handshakeReadClosing(&atem, serverSessionId, true);
}

// Test dropping connection after handshake, responding to closing
void handshake_test7() {
	struct atem_t atem;
	int serverSessionId = handshakeConnect(&atem);
	flushStateDump(&atem);
	awaitOpcode(&atem, serverSessionId, ATEM_CONNECTION_CLOSING);
	handshakeSendClosed(&atem, serverSessionId);
}

