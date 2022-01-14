#include "./atem.h"

// Atem protocol flags
#define ATEM_FLAG_MASK 0x07
#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

// Atem protocol indexes
#define ATEM_INDEX_FLAG 0
#define ATEM_INDEX_SESSION 2
#define ATEM_INDEX_ACK 4
#define ATEM_INDEX_REMOTEID 10
#define ATEM_INDEX_OPCODE 12

// Atem protocol handshake states (more states in header file)
#define ATEM_CONNECTION_SUCCESS 0x02
#define ATEM_CONNECTION_CLOSED 0x05

// Atem protocol lengths
#define ATEM_LEN_HEADER 12
#define ATEM_LEN_SYN 20
#define ATEM_LEN_ACK 12

// Atem remote id range
#define ATEM_LIMIT_REMOTEID 0x8000

// Tally flags
#define ATEM_TALLY_PGM 0x01
#define ATEM_TALLY_PVW 0x02

// Atem and camera control protocol lengths and offsets
#define CC_HEADER_LEN 4
#define CC_CMD_HEADER_LEN 4
#define CC_HEADER_OFFSET -3
#define CC_ATEM_DATA_OFFSET 16

// Buffer to send to establish connection
uint8_t synBuf[ATEM_LEN_SYN] = { ATEM_FLAG_SYN, ATEM_LEN_SYN, 0x74, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	ATEM_CONNECTION_OPENING, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Buffer to modify and send to acknowledge a packet was received
uint8_t ackBuf[ATEM_LEN_ACK] = { ATEM_FLAG_ACK, ATEM_LEN_ACK, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Buffer to modify and send to close connection
uint8_t closeBuf[ATEM_LEN_SYN] = { ATEM_FLAG_SYN, ATEM_LEN_SYN, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	ATEM_CONNECTION_CLOSING, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Resets write buffer to SYN packet for starting handshake
void resetAtemState(struct atem_t *atem) {
	synBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN | ((atem->writeBuf == synBuf) ? ATEM_FLAG_RETRANSMIT : 0);
	atem->writeBuf = synBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Sends a close packet to close the session
void closeAtemConnection(struct atem_t *atem) {
	closeBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN;
	closeBuf[ATEM_INDEX_SESSION] = atem->readBuf[ATEM_INDEX_SESSION];
	closeBuf[ATEM_INDEX_SESSION + 1] = atem->readBuf[ATEM_INDEX_SESSION + 1];
	atem->writeBuf = closeBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Parses an ATEM UDP packet in the atem.readBuf
// Returns: 0 = normal, 1 = connecting client, 3 = rejected, 4 = closing, -1 = error
int8_t parseAtemData(struct atem_t *atem) {
	// Sets length of read buffer
	atem->readLen = (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_MASK) << 8 | atem->readBuf[1];

	// Resends close buffer without processing read data
	if (atem->writeBuf == closeBuf) {
		closeBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN | ATEM_FLAG_RETRANSMIT;
		closeBuf[ATEM_INDEX_SESSION] = atem->readBuf[ATEM_INDEX_SESSION];
		closeBuf[ATEM_INDEX_SESSION + 1] = atem->readBuf[ATEM_INDEX_SESSION + 1];
		atem->cmdIndex = atem->readLen;
		return (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_SYN &&
			atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSED)
			? ATEM_CONNECTION_CLOSED : ATEM_CONNECTION_OK;
	}

	// Sends ACK requested by this packet
	if (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_ACKREQUEST) {
		// Gets remote id of packet
		const uint16_t remoteId = atem->readBuf[ATEM_INDEX_REMOTEID] << 8 |
			atem->readBuf[ATEM_INDEX_REMOTEID + 1];

		// Acknowledge this packet and set it as the last received remote id if next in line
		if (remoteId == (atem->lastRemoteId + 1) % ATEM_LIMIT_REMOTEID) {
			ackBuf[ATEM_INDEX_ACK] = atem->readBuf[ATEM_INDEX_REMOTEID];
			ackBuf[ATEM_INDEX_ACK + 1] = atem->readBuf[ATEM_INDEX_REMOTEID + 1];
			atem->lastRemoteId = remoteId;
		}
		// Sets response acknowledge id to last acknowledged packet id if it is not the next in line
		else {
			ackBuf[ATEM_INDEX_ACK] = atem->lastRemoteId >> 8;
			ackBuf[ATEM_INDEX_ACK + 1] = atem->lastRemoteId & 0xff;
		}

		// Set up for parsing ATEM commands in payload
		atem->cmdIndex = ATEM_LEN_HEADER;
	}
	// Do not process payload or write response on non SYN or ACK request packets
	else if (!(atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_SYN)) {
		atem->cmdIndex = atem->readLen;
		atem->writeLen = 0;
		return ATEM_CONNECTION_ERROR;
	}
	// Sends SYNACK without processing payload to complete handshake
	else if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_SUCCESS) {
		ackBuf[ATEM_INDEX_ACK] = 0x00;
		ackBuf[ATEM_INDEX_ACK + 1] = 0x00;
		atem->lastRemoteId = 0;
		atem->cmdIndex = ATEM_LEN_SYN;
	}
	// Restarts connection on reset, closing or closed opcodes by letting it timeout
	else {
		atem->cmdIndex = ATEM_LEN_SYN;
		atem->writeLen = 0;
		return atem->readBuf[ATEM_INDEX_OPCODE];
	}

	// Copies over session id from incomming packet to ACK packet
	ackBuf[ATEM_INDEX_SESSION] = atem->readBuf[ATEM_INDEX_SESSION];
	ackBuf[ATEM_INDEX_SESSION + 1] = atem->readBuf[ATEM_INDEX_SESSION + 1];

	// Writes ACK buffer to server
	atem->writeBuf = ackBuf;
	atem->writeLen = ATEM_LEN_ACK;
	return ATEM_CONNECTION_OK;
}

// Gets next command name and sets buffer to its payload and length to length of its payload
uint32_t nextAtemCommand(struct atem_t *atem) {
	// Gets start index of command
	const uint16_t index = atem->cmdIndex;

	// Gets command length from command data
	atem->cmdLen = (atem->readBuf[index] << 8) | atem->readBuf[index + 1];

	// Sets pointer to command buffer to start of command data
	atem->cmdBuf = atem->readBuf + index + 8;

	// Increment start index of command with command length to get start index for next command
	atem->cmdIndex += atem->cmdLen;

	// Converts command name to a 32 bit integer for easy comparison
	return (atem->readBuf[index + 4] << 24) | (atem->readBuf[index + 5] << 16) |
		(atem->readBuf[index + 6] << 8) | atem->readBuf[index + 7];
}

// Gets update status for camera index and updates its tally state
// This function has to be called before translateAtemTally
bool tallyHasUpdated(struct atem_t *atem) {
	// Ensures index is within range of tally data length
	if (atem->dest > ((atem->cmdBuf[0] << 8) | atem->cmdBuf[1])) return 0;

	// Stores old states for PGM and PVW tally
	const uint8_t oldTally = atem->pgmTally | atem->pvwTally << 1;

	// Updates states for PGM and PVW tally
	atem->pgmTally = atem->cmdBuf[1 + atem->dest] & ATEM_TALLY_PGM;
	atem->pvwTally = atem->cmdBuf[1 + atem->dest] == ATEM_TALLY_PVW;

	// Returns boolean indicating if tally was updated or not
 	return oldTally != (atem->pgmTally | atem->pvwTally << 1);
}

// Translates tally data from ATEMs protocol to Blackmagic Embedded Tally Control Protocol
void translateAtemTally(struct atem_t *atem) {
	// Gets the number of items in the tally index array
	const uint16_t len = atem->cmdBuf[0] << 8 | atem->cmdBuf[1];

	// Remaps indexes to Blackmagic Embedded Tally Control Protocol
	for (uint16_t i = 2; i <= len; i += 2) {
		atem->cmdBuf[i / 2 + 1] = atem->cmdBuf[i] | atem->cmdBuf[i + 1] << 4;
	}

	// Updates translated pointer, length and sets first byte in translation
	atem->cmdBuf += 1;
	atem->cmdLen = len / 2 + 1;
	atem->cmdBuf[0] = 0x00;
}

// Translates camera control data from ATEMs protocol to Blackmagis SDI camera control protocol
void translateAtemCameraControl(struct atem_t *atem) {
	// Gets data length
	const uint8_t len = atem->cmdBuf[5] + atem->cmdBuf[7] * 2 + atem->cmdBuf[9] * 4;

	// Header
	atem->cmdBuf[CC_HEADER_OFFSET + 0] = atem->cmdBuf[0]; // Destination
	atem->cmdBuf[CC_HEADER_OFFSET + 1] = CC_HEADER_LEN + len; // Length
	atem->cmdBuf[CC_HEADER_OFFSET + 2] = 0x00; // Command
	atem->cmdBuf[CC_HEADER_OFFSET + 3] = 0x00; // Reserved

	// Command
	// Retains byte 1 - 4 to indicate: category, parameter, data type and operation

	// Data
	const uint8_t typeSize = (atem->cmdBuf[5] > 0) + (atem->cmdBuf[7] > 0) * 2 + (atem->cmdBuf[9] > 0) * 4;
	for (uint8_t i = 0; i < len;) {
		for (const uint8_t offset = i; i < offset + typeSize; i++) {
			atem->cmdBuf[CC_HEADER_LEN + CC_CMD_HEADER_LEN + CC_HEADER_OFFSET + offset + typeSize - 1 + offset - i] = atem->cmdBuf[i + CC_ATEM_DATA_OFFSET];
		}
	}

	// Updates translated pointer and length padded to 32 bit boundary
	atem->cmdLen = CC_HEADER_LEN + CC_CMD_HEADER_LEN + (len + 3) / 4 * 4;
	atem->cmdBuf += CC_HEADER_OFFSET;

	// Clears padding bytes
	for (uint8_t i = CC_HEADER_LEN + CC_CMD_HEADER_LEN + len; i < atem->cmdLen; i++) {
		atem->cmdBuf[i] = 0x00;
	}
}
