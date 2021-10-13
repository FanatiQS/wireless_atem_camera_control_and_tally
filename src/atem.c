#include "./atem.h"

// Atem protocol flags
#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

// Atem protocol indexes
#define ATEM_SESSION_INDEX 2
#define ATEM_ACK_INDEX 4

// Atem protocol handshake states
#define ATEM_CONNECTION_SUCCESS 0x02
#define ATEM_CONNECTION_REJECTED 0x03
#define ATEM_CONNECTION_RESTART 0x04

// Atem protocol lengths
#define ATEM_LEN_HEADER 12
#define ATEM_LEN_SYN 20
#define ATEM_LEN_ACK 12

// Buffer to send to establish connection
uint8_t synBuf[ATEM_LEN_SYN] = { ATEM_FLAG_SYN, ATEM_LEN_SYN,
	0x74, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Buffer to modify and send to acknowledge a packet was received
uint8_t ackBuf[ATEM_LEN_ACK] = { ATEM_FLAG_ACK, ATEM_LEN_ACK, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Resets write buffer to SYN packet for starting handshake
void resetAtemState(struct atem_t *atem) {
	synBuf[0] = ATEM_FLAG_SYN | ((atem->writeBuf == synBuf) * ATEM_FLAG_RETRANSMIT);
	atem->writeBuf = synBuf;
	atem->writeLen = ATEM_LEN_SYN;
}

// Parses an ATEM UDP packet in the atem.readBuf
// Returns: 0 = normal, 1 = rejected, -1 = error
int8_t parseAtemData(struct atem_t *atem) {
	// Sets length of read buffer
	atem->readLen = (atem->readBuf[0] & 0x07) << 8 | atem->readBuf[1];

	// Sends ACK requested by this packet
	if (atem->readBuf[0] & ATEM_FLAG_ACKREQUEST) {
		// Gets remote id of packet
		const uint16_t remoteId = atem->readBuf[10] << 8 | atem->readBuf[11];

		// Acknowledge this packet and set it as the last received remote id if next in line
		if (remoteId == (atem->lastRemoteId + 1) % 0x8000) {
			ackBuf[ATEM_ACK_INDEX] = atem->readBuf[10];
			ackBuf[ATEM_ACK_INDEX + 1] = atem->readBuf[11];
			atem->lastRemoteId = remoteId;
		}
		// Sets response acknowledge id to last acknowledged packet id if it is not the next in line
		else {
			ackBuf[ATEM_ACK_INDEX] = atem->lastRemoteId >> 8;
			ackBuf[ATEM_ACK_INDEX + 1] = atem->lastRemoteId & 0xff;
		}

		// Set up for parsing ATEM commands in payload
		atem->cmdIndex = ATEM_LEN_HEADER;
	}
	// Do nothing on non SYN or ACK request packets
	else if (!(atem->readBuf[0] & ATEM_FLAG_SYN)) {
		return -1;
	}
	// Sends SYNACK without processing payload to complete handshake
	else if (atem->readBuf[ATEM_LEN_HEADER] == ATEM_CONNECTION_SUCCESS) {
		ackBuf[ATEM_ACK_INDEX] = atem->lastRemoteId >> 8;
		ackBuf[ATEM_ACK_INDEX + 1] = atem->lastRemoteId & 0xff;
		atem->lastRemoteId = 0x00;
		atem->cmdIndex = ATEM_LEN_SYN;
	}
	// Restarts handshake without processing payload
	else if (atem->readBuf[ATEM_LEN_HEADER] == ATEM_CONNECTION_RESTART) {
		atem->writeBuf = synBuf;
		atem->writeLen = ATEM_LEN_SYN;
		atem->cmdIndex = ATEM_LEN_SYN;
		return 0;
	}
	// Returns 1 on ATEM connection state rejected
	else {
		return 1;
	}

	// Copies over session id from incomming packet to ACK packet
	ackBuf[ATEM_SESSION_INDEX] = atem->readBuf[ATEM_SESSION_INDEX];
	ackBuf[ATEM_SESSION_INDEX + 1] = atem->readBuf[ATEM_SESSION_INDEX + 1];

	// Writes ACK buffer to server
	atem->writeBuf = ackBuf;
	atem->writeLen = ATEM_LEN_ACK;
	return 0;
}

// Gets next command name and sets buffer to its data and length to length of its data
uint32_t parseAtemCommand(struct atem_t *atem) {
	// Gets start index of command
	const uint16_t index = atem->cmdIndex;

	// Gets command length from command data
	atem->cmdLen = (atem->readBuf[index] << 8) | atem->readBuf[index + 1];

	// Sets pointer to command buffer to start of command data
	atem->cmdBuf = atem->readBuf + index + 8;

	// Increment start index of command with command length to get start index for next command
	atem->cmdIndex += atem->cmdLen;

	// Converts command name to a 32 bit integer for easy comparison
	return (atem->readBuf[index + 4] << 24) | (atem->readBuf[index + 5] << 16) | (atem->readBuf[index + 6] << 8) | atem->readBuf[index + 7];
}

// Gets update status for camera index and updates its tally state
int8_t parseAtemTally(struct atem_t *atem, uint16_t index, uint8_t *tally) {
	// Ensures index is within range of tally data length
	if (index > ((atem->cmdBuf[0] << 8) | atem->cmdBuf[1])) return -1;

	// Returns update state and sets new tally state
	return *tally ^ (*tally = (atem->cmdBuf[1 + index] & ATEM_TALLY_PGM) |
		((atem->cmdBuf[1 + index] == ATEM_TALLY_PVW) << 1));
}
