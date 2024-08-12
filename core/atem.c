#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stdbool.h> // bool, false
#include <stddef.h> // NULL

#include "./atem_protocol.h" // ATEM_LEN_SYN, ATEM_INDEX_FLAGS, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_FLAG_SYN, ATEM_INDEX_OPCODE, ATEM_OPCODE_OPEN, ATEM_LEN_ACK, ATEM_FLAG_ACK, ATEM_FLAG_RETX, ATEM_OPCODE_CLOSING, ATEM_OPCODE_CLOSED, ATEM_FLAG_ACKREQ, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, ATEM_LIMIT_REMOTEID, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ATEM_MASK_LEN_HIGH, ATEM_LEN_HEADER, ATEM_OPCODE_ACCEPT, ATEM_OPCODE_REJECT, ATEM_LEN_CMDHEADER, ATEM_OFFSET_CMDNAME
#include "./atem.h" // struct atem, enum atem_status, ATEM_STATUS_CLOSED, ATEM_STATUS_WRITE_ONLY, ATEM_STATUS_WRITE, ATEM_STATUS_NONE, ATEM_STATUS_ACCEPTED, ATEM_STATUS_CLOSING, ATEM_STATUS_REJECTED, ATEM_STATUS_ERROR



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
static ATEM_THREAD_LOCAL uint8_t buf_open[ATEM_LEN_SYN] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
	[ATEM_INDEX_SESSIONID_HIGH] = 0x13,
	[ATEM_INDEX_SESSIONID_LOW] = 0x37,
	[ATEM_INDEX_OPCODE] = ATEM_OPCODE_OPEN
};

// Buffer to modify and send to ATEM when acknowledging a received packet
static ATEM_THREAD_LOCAL uint8_t buf_ack[ATEM_LEN_ACK] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACK,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_ACK
};

// Buffer to modify and send to ATEM to close the connection or respond to closing request
static ATEM_THREAD_LOCAL uint8_t buf_close[ATEM_LEN_SYN] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN
};

// Buffer to request a packet to be retransmitted
static ATEM_THREAD_LOCAL uint8_t buf_retxreq[ATEM_LEN_ACK] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_RETXREQ,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_ACK
};


// Resets write buffer to be a SYN packet for starting handshake
void atem_connection_reset(struct atem* atem) {
	buf_open[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN | ((atem->write_buf == buf_open) ? ATEM_FLAG_RETX : 0);
	atem->write_buf = buf_open;
	atem->write_len = ATEM_LEN_SYN;
}

// Sends a close packet to close the session
void atem_connection_close(struct atem* atem) {
	buf_close[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	buf_close[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
	buf_close[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];
	buf_close[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING;
	atem->write_buf = buf_close;
	atem->write_len = ATEM_LEN_SYN;
}

// Parses a received ATEM UDP packet
enum atem_status atem_parse(struct atem* atem) {
	// Resends close packet without processing potential payload
	if (atem->write_buf == buf_close) {
		// Sets default branch to resend closing request
		buf_close[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN | ATEM_FLAG_RETX;
		buf_close[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING;

		// Processes closing or closed packet
		if (atem->read_buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN) {
			// Returns closed status for response to close request
			if (atem->read_buf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSED) {
				return ATEM_STATUS_CLOSED;
			}

			// Sets close packet to be closed response to closing request
			if (atem->read_buf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSING) {
				buf_close[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
				buf_close[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSED;
			}
		}

		// Copies over session id from incomming packet to close packet
		buf_close[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
		buf_close[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

		return ATEM_STATUS_WRITE_ONLY;
	}

	// Responds with ACK packet to packet requesting it
	if (atem->read_buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACKREQ) {
		// Gets remote id in this packet and next remote id in sequence
		const uint16_t remote_id_next = (atem->remote_id_last + 1) & ATEM_LIMIT_REMOTEID;
		const uint16_t remote_id_recved = (uint16_t)(atem->read_buf[ATEM_INDEX_REMOTEID_HIGH] << 8) |
			atem->read_buf[ATEM_INDEX_REMOTEID_LOW];

		// Acknowledges this packet if it is next in line
		if (remote_id_recved == remote_id_next) {
			// Updates last acknowledged remote id
			atem->remote_id_last = remote_id_recved;

			// Sets ACK response to be sent as response
			atem->write_buf = buf_ack;

			// Copies over session id from incomming packet to ACK response
			buf_ack[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
			buf_ack[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

			// Copies over remote id from incomming packets to ACK responses ack id
			buf_ack[ATEM_INDEX_ACKID_HIGH] = atem->read_buf[ATEM_INDEX_REMOTEID_HIGH];
			buf_ack[ATEM_INDEX_ACKID_LOW] = atem->read_buf[ATEM_INDEX_REMOTEID_LOW];

			// Sets length of read buffer
			atem->read_len = (uint16_t)(atem->read_buf[ATEM_INDEX_LEN_HIGH] << 8 |
				atem->read_buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;

			// Sets up for parsing ATEM commands in payload
			atem->cmd_index = ATEM_LEN_HEADER;

			// Process payload if available
			return ATEM_STATUS_WRITE;
		}
		// Sends retransmit request if received remote id is closer to being ahead than behind
		else if (((remote_id_recved - remote_id_next) & 0x7fff) < (0x7fff / 2)) {
			// Sets RETX response to be sent as response
			atem->write_buf = buf_retxreq;

			// Copies over session id from incomming packet to RETX response
			buf_retxreq[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
			buf_retxreq[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

			// Sets RETX responses local id to next remote id in sequence
			buf_retxreq[ATEM_INDEX_LOCALID_HIGH] = remote_id_next >> 8;
			buf_retxreq[ATEM_INDEX_LOCALID_LOW] = remote_id_next & 0xff;
		}
		// Sends acknowledgement for last acknowledged remote id if received remote id is behind in sequence
		else {
			// Sets ACK response to be sent as response
			atem->write_buf = buf_ack;

			// Copies over session id from incomming packet to ACK response
			buf_ack[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
			buf_ack[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

			// Sets ACK responses ack id to last remote id acknowledged
			buf_ack[ATEM_INDEX_ACKID_HIGH] = atem->remote_id_last >> 8;
			buf_ack[ATEM_INDEX_ACKID_LOW] = atem->remote_id_last & 0xff;
		}

		// Do not process payload for packets with remote id not next in sequence
		return ATEM_STATUS_WRITE_ONLY;
	}
	// Ignores packets that are not ACK or SYN
	else if (!(atem->read_buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN)) {
		return ATEM_STATUS_NONE;
	}
	// Responds to accept SYN/ACK packet to complete opening handshake
	else if (atem->read_buf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_ACCEPT) {
		// Copies over session id from incomming packet to ACK response and clears ack id
		buf_ack[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
		buf_ack[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];
		buf_ack[ATEM_INDEX_ACKID_HIGH] = 0x00;
		buf_ack[ATEM_INDEX_ACKID_LOW] = 0x00;

		// Initializes state for the first accept packet only
		if (atem->write_buf == buf_ack) {
			return ATEM_STATUS_WRITE_ONLY;
		}
		atem->write_buf = buf_ack;
		atem->write_len = ATEM_LEN_ACK;
		atem->remote_id_last = 0;
		return ATEM_STATUS_ACCEPTED;
	}
	// Responds to closing request with closed response
	else if (atem->read_buf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_CLOSING) {
		buf_close[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
		buf_close[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
		buf_close[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];
		buf_close[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSED;
		atem->write_buf = buf_close;
		atem->write_len = ATEM_LEN_SYN;
		return ATEM_STATUS_CLOSING;
	}
	// Returns reject status
	else if (atem->read_buf[ATEM_INDEX_OPCODE] == ATEM_OPCODE_REJECT) {
		atem->write_buf = NULL;
		return ATEM_STATUS_REJECTED;
	}

	// Returns status indicating the opcode was invalid
	return ATEM_STATUS_ERROR;
}

// Gets next command name and sets command buffer to a pointer to its data
uint32_t atem_cmd_next(struct atem* atem) {
	// Gets start index of command in read buffer
	const uint16_t index = atem->cmd_index;

	// Increment start index of command with command length to get start index for next command
	atem->cmd_len = (uint16_t)(atem->read_buf[index] << 8) | atem->read_buf[index + 1];
	atem->cmd_index += atem->cmd_len;

	// Sets pointer to command to start of command data
	atem->cmd_buf = atem->read_buf + index + ATEM_LEN_CMDHEADER;

	// Converts command name to a 32 bit integer for easy comparison
	return (uint32_t)((atem->read_buf[index + ATEM_OFFSET_CMDNAME + 0] << 24) |
		(atem->read_buf[index + ATEM_OFFSET_CMDNAME + 1] << 16) |
		(atem->read_buf[index + ATEM_OFFSET_CMDNAME + 2] << 8) |
		atem->read_buf[index + ATEM_OFFSET_CMDNAME + 3]);
}



// Gets update status for camera index and updates its tally state
bool atem_tally_updated(struct atem* atem) {
	// Ensures destination is within range of tally data length
	if (atem->cmd_buf[TALLY_INDEX_LEN_HIGH] != 0 || atem->cmd_buf[TALLY_INDEX_LEN_LOW] < atem->dest) {
		return false;
	}

	// Stores old states for PGM and PVW tally
	const uint8_t tally_old = (uint8_t)(atem->tally_pgm | atem->tally_pvw << 1);

	// Updates states for PGM and PVW tally
	atem->tally_pgm = atem->cmd_buf[TALLY_OFFSET + atem->dest] & TALLY_FLAG_PGM;
	atem->tally_pvw = atem->cmd_buf[TALLY_OFFSET + atem->dest] == TALLY_FLAG_PVW;

	// Returns boolean indicating if tally was updated or not
	return tally_old != (atem->tally_pgm | atem->tally_pvw << 1);
}

// Translates camera control data from ATEMs protocol to Blackmagis SDI camera control protocol
void atem_cc_translate(struct atem* atem) {
	// Gets length of available data
	const uint8_t len = atem->cmd_buf[5] + atem->cmd_buf[7] * 2 + atem->cmd_buf[9] * 4;

	// Header
	atem->cmd_buf[CC_HEADER_OFFSET + 0] = atem->cmd_buf[0]; // Destination
	atem->cmd_buf[CC_HEADER_OFFSET + 1] = CC_HEADER_LEN + len; // Length
	atem->cmd_buf[CC_HEADER_OFFSET + 2] = 0x00; // Command
	atem->cmd_buf[CC_HEADER_OFFSET + 3] = 0x00; // Reserved

	// Command
	// Retains byte 1 - 4 to indicate: category, parameter, data type and operation

	// Data
	const uint8_t typeSize = (atem->cmd_buf[5] > 0) + (atem->cmd_buf[7] > 0) * 2 + (atem->cmd_buf[9] > 0) * 4;
	for (uint8_t i = 0; i < len;) {
		for (const uint8_t offset = i; i < offset + typeSize; i++) {
			atem->cmd_buf[CC_HEADER_LEN + CC_CMD_HEADER_LEN + CC_HEADER_OFFSET + offset + typeSize - 1 + offset - i] = atem->cmd_buf[i + CC_ATEM_DATA_OFFSET];
		}
	}

	// Updates translated pointer and length padded to 32 bit boundary
	atem->cmd_len = CC_HEADER_LEN + CC_CMD_HEADER_LEN + (len + 3) / 4 * 4;
	atem->cmd_buf += CC_HEADER_OFFSET;

	// Clears padding bytes
	for (uint16_t i = CC_HEADER_LEN + CC_CMD_HEADER_LEN + len; i < atem->cmd_len; i++) {
		atem->cmd_buf[i] = 0x00;
	}
}
