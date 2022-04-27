#include "./atem.h"

// Mask to use when filtering out flags
#define ATEM_MASK_FLAG 0x07

// Atem protocol flags
#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

// Atem protocol indexes
#define ATEM_INDEX_FLAG 0
#define ATEM_INDEX_LEN 1
#define ATEM_INDEX_SESSION_HIGH 2
#define ATEM_INDEX_SESSION_LOW 3
#define ATEM_INDEX_ACK_HIGH 4
#define ATEM_INDEX_ACK_LOW 5
#define ATEM_INDEX_REMOTEID_HIGH 10
#define ATEM_INDEX_REMOTEID_LOW 11
#define ATEM_INDEX_OPCODE 12

// Atem protocol handshake states (more states in header file)
#define ATEM_CONNECTION_OPENING 0x01
#define ATEM_CONNECTION_SUCCESS 0x02

// Atem protocol lengths
#define ATEM_LEN_HEADER 12
#define ATEM_LEN_SYN 20
#define ATEM_LEN_ACK 12
#define ATEM_LEN_CMDHEADER 8

// Atem remote id range
#define ATEM_LIMIT_REMOTEID 0x8000

// Tally flags indicating the status
#define TALLY_FLAG_PGM 0x01
#define TALLY_FLAG_PVW 0x02

// Indexes of length for number of tally values
#define TALLY_INDEX_LEN_HIGH 0
#define TALLY_INDEX_LEN_LOW 1

// Offset for when using tally index as command index
#define TALLY_OFFSET 1

// Atem and camera control protocol lengths and offsets
#define CC_HEADER_LEN 4
#define CC_CMD_HEADER_LEN 4
#define CC_HEADER_OFFSET -3
#define CC_ATEM_DATA_OFFSET 16

// Buffer to send to ATEM when establishing connection
uint8_t synBuf[ATEM_LEN_SYN] = {
	[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN,
	[ATEM_INDEX_LEN] = ATEM_LEN_SYN,
	[ATEM_INDEX_SESSION_HIGH] = 0x74,
	[ATEM_INDEX_SESSION_LOW] = 0x40,
	[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_OPENING
};

// Buffer to modify and send to ATEM when acknowledging a received packet
uint8_t ackBuf[ATEM_LEN_ACK] = {
	[ATEM_INDEX_FLAG] = ATEM_FLAG_ACK,
	[ATEM_INDEX_LEN] = ATEM_LEN_ACK
};

// Buffer to modify and send to ATEM to close the connection
uint8_t closeBuf[ATEM_LEN_SYN] = {
	[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN,
	[ATEM_INDEX_LEN] = ATEM_LEN_SYN,
	[ATEM_INDEX_OPCODE] = ATEM_CONNECTION_CLOSING
};

// Resets write buffer to be a SYN packet for starting handshake
void resetAtemState(struct atem_t *atem) {
	synBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN | ((atem->writeBuf == synBuf) ? ATEM_FLAG_RETRANSMIT : 0);
	atem->writeBuf = synBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Sends a close packet to close the session
void closeAtemConnection(struct atem_t *atem) {
	closeBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN;
	closeBuf[ATEM_INDEX_SESSION_HIGH] = atem->readBuf[ATEM_INDEX_SESSION_HIGH];
	closeBuf[ATEM_INDEX_SESSION_LOW] = atem->readBuf[ATEM_INDEX_SESSION_LOW];
	atem->writeBuf = closeBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Parses a received ATEM UDP packet
int8_t parseAtemData(struct atem_t *atem) {
	// Sets length of read buffer
	atem->readLen = (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_MASK_FLAG) << 8 | atem->readBuf[1];

	// Resends close buffer without processing read data or returns closed state
	if (atem->writeBuf == closeBuf) {
		atem->cmdIndex = atem->readLen;
		if (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_SYN &&
			atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_CLOSED
		) {
			resetAtemState(atem);
			return ATEM_CONNECTION_CLOSED;
		}
		closeBuf[ATEM_INDEX_FLAG] = ATEM_FLAG_SYN | ATEM_FLAG_RETRANSMIT;
		closeBuf[ATEM_INDEX_SESSION_HIGH] = atem->readBuf[ATEM_INDEX_SESSION_HIGH];
		closeBuf[ATEM_INDEX_SESSION_LOW] = atem->readBuf[ATEM_INDEX_SESSION_LOW];
		return ATEM_CONNECTION_OK;
	}

	// Responds with an ACK as requested by this packet
	if (atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_ACKREQUEST) {
		// Gets remote id of this packet
		const uint16_t remoteId = atem->readBuf[ATEM_INDEX_REMOTEID_HIGH] << 8 |
			atem->readBuf[ATEM_INDEX_REMOTEID_LOW];

		// Acknowledges this packet if it is next in line
		if (remoteId == (atem->lastRemoteId + 1) % ATEM_LIMIT_REMOTEID) {
			ackBuf[ATEM_INDEX_ACK_HIGH] = atem->readBuf[ATEM_INDEX_REMOTEID_HIGH];
			ackBuf[ATEM_INDEX_ACK_LOW] = atem->readBuf[ATEM_INDEX_REMOTEID_LOW];
			atem->lastRemoteId = remoteId;

			// Sets up for parsing ATEM commands in payload
			atem->cmdIndex = ATEM_LEN_HEADER;
		}
		// Sets response acknowledge id to last acknowledged packet id if it is not the next in line
		else {
			ackBuf[ATEM_INDEX_ACK_HIGH] = atem->lastRemoteId >> 8;
			ackBuf[ATEM_INDEX_ACK_LOW] = atem->lastRemoteId & 0xff;

			// Sets up to not parse ATEM commands for already processed packet id
			atem->cmdIndex = atem->readLen;
		}
	}
	// Does not process payload or write response on non SYN or ACK request packets
	else if (!(atem->readBuf[ATEM_INDEX_FLAG] & ATEM_FLAG_SYN)) {
		atem->cmdIndex = atem->readLen;
		atem->writeLen = 0;
		return ATEM_CONNECTION_ERROR;
	}
	// Responds to SYNACK without processing payload to complete handshake
	else if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_CONNECTION_SUCCESS) {
		ackBuf[ATEM_INDEX_ACK_HIGH] = 0x00;
		ackBuf[ATEM_INDEX_ACK_LOW] = 0x00;
		atem->lastRemoteId = 0;
		atem->cmdIndex = ATEM_LEN_SYN;
	}
	// Restarts connection on reset, closing or closed opcodes by letting it timeout
	else {
		atem->cmdIndex = ATEM_LEN_SYN;
		atem->writeLen = 0;
		return atem->readBuf[ATEM_INDEX_OPCODE];
	}

	// Copies over session id from incomming packet to ACK response
	ackBuf[ATEM_INDEX_SESSION_HIGH] = atem->readBuf[ATEM_INDEX_SESSION_HIGH];
	ackBuf[ATEM_INDEX_SESSION_LOW] = atem->readBuf[ATEM_INDEX_SESSION_LOW];

	// Sets ACK buffer to be written
	atem->writeBuf = ackBuf;
	atem->writeLen = ATEM_LEN_ACK;
	return ATEM_CONNECTION_OK;
}

// Gets next command name and sets command buffer to a pointer to its data
uint32_t nextAtemCommand(struct atem_t *atem) {
	// Gets start index of command in read buffer
	const uint16_t index = atem->cmdIndex;

	// Increment start index of command with command length to get start index for next command
	atem->cmdIndex += (atem->readBuf[index] << 8) | atem->readBuf[index + 1];

	// Sets pointer to command to start of command data
	atem->cmdBuf = atem->readBuf + index + ATEM_LEN_CMDHEADER;

	// Converts command name to a 32 bit integer for easy comparison
	return (atem->readBuf[index + 4] << 24) | (atem->readBuf[index + 5] << 16) |
		(atem->readBuf[index + 6] << 8) | atem->readBuf[index + 7];
}

// Gets update status for camera index and updates its tally state
// This function has to be called before translateAtemTally
bool tallyHasUpdated(struct atem_t *atem) {
	// Ensures destination is within range of tally data length
	if (atem->dest > ((atem->cmdBuf[TALLY_INDEX_LEN_HIGH] << 8) |
		atem->cmdBuf[TALLY_INDEX_LEN_LOW])) return false;

	// Stores old states for PGM and PVW tally
	const uint8_t oldTally = atem->pgmTally | atem->pvwTally << 1;

	// Updates states for PGM and PVW tally
	atem->pgmTally = atem->cmdBuf[TALLY_OFFSET + atem->dest] & TALLY_FLAG_PGM;
	atem->pvwTally = atem->cmdBuf[TALLY_OFFSET + atem->dest] == TALLY_FLAG_PVW;

	// Returns boolean indicating if tally was updated or not
 	return oldTally != (atem->pgmTally | atem->pvwTally << 1);
}

// Translates tally data from ATEMs protocol to Blackmagic Embedded Tally Control Protocol
void translateAtemTally(struct atem_t *atem) {
	// Gets the number of items in the tally index array
	const uint16_t len = atem->cmdBuf[TALLY_INDEX_LEN_HIGH] << 8 |
		atem->cmdBuf[TALLY_INDEX_LEN_LOW];

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
	// Gets length of available data
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
