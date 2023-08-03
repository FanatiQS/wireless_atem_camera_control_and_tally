#include <stdint.h> // uint8_t

#include "./atem_protocol.h"
#include "./atem.h"



// Buffer to send to ATEM when establishing connection
static uint8_t openBuf[ATEM_LEN_SYN] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
	[ATEM_INDEX_SESSIONID_HIGH] = 0x13,
	[ATEM_INDEX_SESSIONID_LOW] = 0x37,
	[ATEM_INDEX_OPCODE] = ATEM_OPCODE_OPEN
};

// Buffer to modify and send to ATEM when acknowledging a received packet
static uint8_t ackBuf[ATEM_LEN_ACK] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACK,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_ACK
};

// Buffer to modify and send to ATEM to close the connection or respond to closing request
static uint8_t closeBuf[ATEM_LEN_SYN] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
};



// Resets write buffer to be a SYN packet for starting handshake
void atem_connection_reset(struct atem_t *atem) {
	openBuf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN | ((atem->writeBuf == openBuf) ? ATEM_FLAG_RETX : 0);
	atem->writeBuf = openBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Sends a close packet to close the session
void atem_connection_close(struct atem_t *atem) {
	closeBuf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	closeBuf[ATEM_INDEX_SESSIONID_HIGH] = atem->readBuf[ATEM_INDEX_SESSIONID_HIGH];
	closeBuf[ATEM_INDEX_SESSIONID_LOW] = atem->readBuf[ATEM_INDEX_SESSIONID_LOW];
	closeBuf[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING;
	atem->writeBuf = closeBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Parses a received ATEM UDP packet
enum atem_status_t atem_parse(struct atem_t *atem) {
	// Resends close packet without processing potential payload
	if (atem->writeBuf == closeBuf) {
		// Sets default branch to resend closing request
		closeBuf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN | ATEM_FLAG_RETX;
		closeBuf[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING;

		// Processes closing or closed packet
		if (atem->readBuf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN) {
			// Returns closed status for response to close request
			if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSED) {
				return ATEM_STATUS_CLOSED;
			}

			// Sets close packet to be closed response to closing request
			if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSING) {
				closeBuf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
				closeBuf[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSED;
			}
		}

		// Copies over session id from incomming packet to close packet
		closeBuf[ATEM_INDEX_SESSIONID_HIGH] = atem->readBuf[ATEM_INDEX_SESSIONID_HIGH];
		closeBuf[ATEM_INDEX_SESSIONID_LOW] = atem->readBuf[ATEM_INDEX_SESSIONID_LOW];

		return ATEM_STATUS_WRITE_ONLY;
	}

	// Responds with ACK packet to packet requesting it
	if (atem->readBuf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACKREQ) {
		// Gets remote id of this packet
		const uint16_t remoteId = (uint16_t)(atem->readBuf[ATEM_INDEX_REMOTEID_HIGH] << 8) |
			atem->readBuf[ATEM_INDEX_REMOTEID_LOW];

		// Copies over session id from incomming packet to ACK response
		ackBuf[ATEM_INDEX_SESSIONID_HIGH] = atem->readBuf[ATEM_INDEX_SESSIONID_HIGH];
		ackBuf[ATEM_INDEX_SESSIONID_LOW] = atem->readBuf[ATEM_INDEX_SESSIONID_LOW];

		// Acknowledges this packet if it is next in line
		if (remoteId == ((atem->lastRemoteId + 1) & ATEM_LIMIT_REMOTEID)) {
			// Copies over remote id from incomming packets to ACK responses ack id
			ackBuf[ATEM_INDEX_ACKID_HIGH] = atem->readBuf[ATEM_INDEX_REMOTEID_HIGH];
			ackBuf[ATEM_INDEX_ACKID_LOW] = atem->readBuf[ATEM_INDEX_REMOTEID_LOW];

			// Updates last acknolwedged remote id
			atem->lastRemoteId = remoteId;

			// Sets length of read buffer
			atem->readLen = (uint16_t)((atem->readBuf[ATEM_INDEX_LEN_HIGH] & ATEM_MASK_LEN_HIGH) << 8) |
				atem->readBuf[ATEM_INDEX_LEN_LOW];

			// Sets up for parsing ATEM commands in payload
			atem->cmdIndex = ATEM_LEN_HEADER;

			// Process payload if available
			return ATEM_STATUS_WRITE;
		}
		// Sets response acknowledge id to last acknowledged packet id if it is not the next in line
		else {
			ackBuf[ATEM_INDEX_ACKID_HIGH] = atem->lastRemoteId >> 8;
			ackBuf[ATEM_INDEX_ACKID_LOW] = atem->lastRemoteId & 0xff;

			// Do not process payload for already received remote id
			return ATEM_STATUS_WRITE_ONLY;
		}
	}
	// Ignores packets that are not ACK or SYN
	else if (!(atem->readBuf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN)) {
		return ATEM_STATUS_NONE;
	}
	// Responds to accept SYN/ACK packet to complete opening handshake
	else if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_ACCEPT) {
		// Copies over session id from incomming packet to ACK response and clears ack id
		ackBuf[ATEM_INDEX_SESSIONID_HIGH] = atem->readBuf[ATEM_INDEX_SESSIONID_HIGH];
		ackBuf[ATEM_INDEX_SESSIONID_LOW] = atem->readBuf[ATEM_INDEX_SESSIONID_LOW];
		ackBuf[ATEM_INDEX_ACKID_HIGH] = 0x00;
		ackBuf[ATEM_INDEX_ACKID_LOW] = 0x00;

		// Initializes state for the first accept packet only
		if (atem->writeBuf == ackBuf) return ATEM_STATUS_WRITE_ONLY;
		atem->writeBuf = ackBuf;
		atem->writeLen = ATEM_LEN_ACK;
		atem->lastRemoteId = 0;
		return ATEM_STATUS_ACCEPTED;
	}
	// Responds to closing request with closed response
	else if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSING) {
		closeBuf[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
		closeBuf[ATEM_INDEX_SESSIONID_HIGH] = atem->readBuf[ATEM_INDEX_SESSIONID_HIGH];
		closeBuf[ATEM_INDEX_SESSIONID_LOW] = atem->readBuf[ATEM_INDEX_SESSIONID_LOW];
		closeBuf[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSED;
		atem->writeBuf = closeBuf;
		atem->writeLen = ATEM_LEN_SYN;
		return ATEM_STATUS_CLOSING;
	}
	// Returns reject status
	else if (atem->readBuf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_REJECT) {
		return ATEM_STATUS_REJECTED;
	}

	// Returns status indicating the opcode was invalid
	return ATEM_STATUS_ERROR;
}

// Gets next command name and sets command buffer to a pointer to its data
uint32_t atem_cmd_next(struct atem_t *atem) {
	// Gets start index of command in read buffer
	const uint16_t index = atem->cmdIndex;

	// Increment start index of command with command length to get start index for next command
	atem->cmdLen = (uint16_t)(atem->readBuf[index] << 8) | atem->readBuf[index + 1];
	atem->cmdIndex += atem->cmdLen;

	// Sets pointer to command to start of command data
	atem->cmdBuf = atem->readBuf + index + ATEM_LEN_CMDHEADER;

	// Converts command name to a 32 bit integer for easy comparison
	return (uint32_t)((atem->readBuf[index + ATEM_OFFSET_CMDNAME + 0] << 24) |
		(atem->readBuf[index + ATEM_OFFSET_CMDNAME + 1] << 16) |
		(atem->readBuf[index + ATEM_OFFSET_CMDNAME + 2] << 8) |
		atem->readBuf[index + ATEM_OFFSET_CMDNAME + 3]);
}



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



// Gets update status for camera index and updates its tally state
bool atem_tally_updated(struct atem_t *atem) {
	// Ensures destination is within range of tally data length
	if (atem->cmdBuf[TALLY_INDEX_LEN_HIGH] != 0 || atem->cmdBuf[TALLY_INDEX_LEN_LOW] < atem->dest) {
		return false;
	}

	// Stores old states for PGM and PVW tally
	const uint8_t oldTally = (uint8_t)(atem->pgmTally | atem->pvwTally << 1);

	// Updates states for PGM and PVW tally
	atem->pgmTally = atem->cmdBuf[TALLY_OFFSET + atem->dest] & TALLY_FLAG_PGM;
	atem->pvwTally = atem->cmdBuf[TALLY_OFFSET + atem->dest] == TALLY_FLAG_PVW;

	// Returns boolean indicating if tally was updated or not
 	return oldTally != (atem->pgmTally | atem->pvwTally << 1);
}

// Translates tally data from ATEMs protocol to Blackmagic Embedded Tally Control Protocol
void atem_tally_translate(struct atem_t *atem) {
	// Gets the number of items in the tally index array
	const uint16_t len = (uint16_t)(atem->cmdBuf[TALLY_INDEX_LEN_HIGH] << 8) |
		atem->cmdBuf[TALLY_INDEX_LEN_LOW];

	// Remaps indexes to Blackmagic Embedded Tally Control Protocol
	for (uint16_t i = 2; i <= len; i += 2) {
		atem->cmdBuf[i / 2 + 1] = atem->cmdBuf[i] | (uint8_t)(atem->cmdBuf[i + 1] << 4);
	}

	// Updates translated pointer, length and sets first byte in translation
	atem->cmdBuf += 1;
	atem->cmdLen = len / 2 + 1;
	atem->cmdBuf[0] = 0x00;
}

// Translates camera control data from ATEMs protocol to Blackmagis SDI camera control protocol
void atem_cc_translate(struct atem_t *atem) {
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
