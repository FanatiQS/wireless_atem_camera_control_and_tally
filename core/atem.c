#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stdbool.h> // bool, false
#include <stddef.h> // NULL
#include <assert.h> // assert

#include "./atem_protocol.h" // ATEM_LEN_SYN, ATEM_INDEX_FLAGS, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_FLAG_SYN, ATEM_INDEX_OPCODE, ATEM_OPCODE_OPEN, ATEM_FLAG_ACK, ATEM_FLAG_RETX, ATEM_OPCODE_CLOSING, ATEM_OPCODE_CLOSED, ATEM_FLAG_ACKREQ, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, ATEM_LIMIT_REMOTEID, ATEM_INDEX_ACKID_HIGH, ATEM_INDEX_ACKID_LOW, ATEM_MASK_LEN_HIGH, ATEM_LEN_HEADER, ATEM_OPCODE_ACCEPT, ATEM_OPCODE_REJECT, ATEM_LEN_CMDHEADER, ATEM_OFFSET_CMDNAME
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

// Makes static buffers thread safe if ATEM_THREAD_SAFE is set using thread_local or ATEM_THREAD_LOCAL if defined
#if !ATEM_THREAD_SAFE
#define thread_local
#elif defined(ATEM_THREAD_LOCAL)
#define thread_local ATEM_THREAD_LOCAL
#elif __STDC_VERSION__ < 202311
#include <threads.h> // thread_local
#endif // !ATEM_THREAD_SAFE



// Buffer to send to ATEM when establishing connection
static thread_local uint8_t buf_open[ATEM_LEN_SYN] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
	[ATEM_INDEX_SESSIONID_HIGH] = 0x13,
	[ATEM_INDEX_SESSIONID_LOW] = 0x37,
	[ATEM_INDEX_OPCODE] = ATEM_OPCODE_OPEN
};

// Buffer to modify and send to ATEM when acknowledging a received packet
static thread_local uint8_t buf_ack[ATEM_LEN_HEADER] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACK,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_HEADER
};

// Buffer to modify and send to ATEM to close the connection or respond to closing request
static thread_local uint8_t buf_close[ATEM_LEN_SYN] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN
};

// Buffer to request a packet to be retransmitted
static thread_local uint8_t buf_retxreq[ATEM_LEN_HEADER] = {
	[ATEM_INDEX_FLAGS] = ATEM_FLAG_RETXREQ,
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_HEADER
};



// Sets write buffer to be an opening handshake SYN packet to start a new connection
void atem_connection_open(struct atem* atem) {
	assert(atem != NULL);
	buf_open[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN | ((atem->write_buf == buf_open) ? ATEM_FLAG_RETX : 0);
	atem->write_buf = buf_open;
	atem->write_len = ATEM_LEN_SYN;
}

// Sends a close packet to close the session
void atem_connection_close(struct atem* atem) {
	assert(atem != NULL);
	buf_close[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	buf_close[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
	buf_close[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];
	buf_close[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING;
	atem->write_buf = buf_close;
	atem->write_len = ATEM_LEN_SYN;
}

// Parses a received ATEM UDP packet
enum atem_status atem_parse(struct atem* atem) {
	assert(atem != NULL);

	// Resends close packet without processing potential payload
	if (atem->write_buf == buf_close) {
		assert(atem->write_len == ATEM_LEN_SYN);

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

		// Copies over session id from incoming packet to close packet
		buf_close[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
		buf_close[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

		return ATEM_STATUS_WRITE_ONLY;
	}

	// Responds with ACK packet to packet requesting it
	if (atem->read_buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACKREQ) {
		// Gets remote id in this packet and next remote id in sequence
		const uint16_t remote_id_next = (atem->remote_id_last + 1) & ATEM_LIMIT_REMOTEID;
		const uint16_t remote_id_recved = ((atem->read_buf[ATEM_INDEX_REMOTEID_HIGH] << 8) |
			atem->read_buf[ATEM_INDEX_REMOTEID_LOW]) & ATEM_LIMIT_REMOTEID;

		// Acknowledges this packet if it is next in line
		if (remote_id_recved == remote_id_next) {
			// Updates last acknowledged remote id
			atem->remote_id_last = remote_id_recved;

			// Sets ACK response to be sent as response
			atem->write_buf = buf_ack;

			// Copies over session id from incoming packet to ACK response
			buf_ack[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
			buf_ack[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];

			// Copies over remote id from incoming packets to ACK responses ack id
			buf_ack[ATEM_INDEX_ACKID_HIGH] = atem->read_buf[ATEM_INDEX_REMOTEID_HIGH];
			buf_ack[ATEM_INDEX_ACKID_LOW] = atem->read_buf[ATEM_INDEX_REMOTEID_LOW];

			// Sets length of read buffer
			atem->read_len = (atem->read_buf[ATEM_INDEX_LEN_HIGH] << 8 |
				atem->read_buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;

			// Sets up for parsing ATEM commands in payload
			atem->cmd_index_next = ATEM_LEN_HEADER;

			// Process payload if available
			return ATEM_STATUS_WRITE;
		}
		// Sends retransmit request if received remote id is closer to being ahead than behind
		else if (((remote_id_recved - remote_id_next) & ATEM_LIMIT_REMOTEID) < (ATEM_LIMIT_REMOTEID / 2)) {
			// Sets RETX response to be sent as response
			atem->write_buf = buf_retxreq;

			// Copies over session id from incoming packet to RETX response
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

			// Copies over session id from incoming packet to ACK response
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
		// Copies over session id from incoming packet to ACK response and clears ack id
		buf_ack[ATEM_INDEX_SESSIONID_HIGH] = atem->read_buf[ATEM_INDEX_SESSIONID_HIGH];
		buf_ack[ATEM_INDEX_SESSIONID_LOW] = atem->read_buf[ATEM_INDEX_SESSIONID_LOW];
		buf_ack[ATEM_INDEX_ACKID_HIGH] = 0x00;
		buf_ack[ATEM_INDEX_ACKID_LOW] = 0x00;

		// Initializes state for the first accept packet only
		if (atem->write_buf == buf_ack) {
			return ATEM_STATUS_WRITE_ONLY;
		}
		atem->write_buf = buf_ack;
		atem->write_len = ATEM_LEN_HEADER;
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
	assert(atem != NULL);
	assert(atem->read_buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACKREQ);
	assert(atem->read_len >= atem->cmd_index_next);
	assert(atem->read_len <= ATEM_PACKET_LEN_MAX);
	assert(atem->cmd_index_next >= ATEM_LEN_HEADER);
	assert(atem->cmd_index_next <= ATEM_PACKET_LEN_MAX);

	// Gets pointer to command in read buffer
	uint8_t* const buf = &atem->read_buf[atem->cmd_index_next];

	// Sets index of next command
	uint16_t cmd_len = (uint16_t)(buf[0] << 8 | buf[1]);
	atem->cmd_index_next += cmd_len;

	// Sets command buffer and length
	atem->cmd_payload_len = cmd_len - ATEM_LEN_CMDHEADER;
	atem->cmd_payload_buf = buf + ATEM_LEN_CMDHEADER;

	// Converts command name to a 32 bit integer for easy comparison
	return (uint32_t)(buf[4] << 24 | buf[5] << 16 | buf[6] << 8 | buf[7]);
}



// Gets update status for camera index and updates its tally state
bool atem_tally_updated(struct atem* atem) {
	assert(atem != NULL);
	assert(atem->cmd_payload_buf >= &atem->read_buf[ATEM_LEN_HEADER]);
	assert(atem->cmd_payload_buf < &atem->read_buf[ATEM_PACKET_LEN_MAX]);

	// Ensures destination is within range of tally data length
	if (atem->cmd_payload_buf[TALLY_INDEX_LEN_HIGH] != 0 || atem->cmd_payload_buf[TALLY_INDEX_LEN_LOW] < atem->dest) {
		return false;
	}

	// Stores old states for PGM and PVW tally
	const bool tally_pgm_old = atem->tally_pgm;
	const bool tally_pvw_old = atem->tally_pvw;

	// Updates states for PGM and PVW tally
	atem->tally_pgm = atem->cmd_payload_buf[TALLY_OFFSET + atem->dest] & TALLY_FLAG_PGM;
	atem->tally_pvw = atem->cmd_payload_buf[TALLY_OFFSET + atem->dest] == TALLY_FLAG_PVW;

	// Returns boolean indicating if tally was updated or not
	return (tally_pgm_old != atem->tally_pgm) || (tally_pvw_old != atem->tally_pvw);
}

// Translates camera control data from ATEMs protocol to Blackmagics SDI camera control protocol
void atem_cc_translate(struct atem* atem) {
	assert(atem != NULL);
	assert(atem->cmd_payload_buf >= &atem->read_buf[ATEM_LEN_HEADER]);
	assert(atem->cmd_payload_buf < &atem->read_buf[ATEM_PACKET_LEN_MAX]);

	// Gets length of payload and size of each value
	const uint8_t count8 = atem->cmd_payload_buf[5];
	const uint8_t count16 = atem->cmd_payload_buf[7];
	const uint8_t count32 = atem->cmd_payload_buf[9];
	const uint8_t width = (count8 > 0) + (count16 > 0) * 2 + (count32 > 0) * 4;
	const uint8_t len = count8 + count16 * 2 + count32 * 4;

	// Sets SDI header
	uint8_t* const sdi_buf = &atem->cmd_payload_buf[CC_HEADER_OFFSET];
	sdi_buf[0] = atem->cmd_payload_buf[0]; // Destination
	sdi_buf[1] = CC_CMD_HEADER_LEN + len; // Length
	sdi_buf[2] = 0x00; // Command
	sdi_buf[3] = 0x00; // Reserved

	// Sets SDI command header
	// Retains byte 1 - 4 to indicate: category, parameter, data type and operation

	// Updates byte order from big endian from ATEM to little endian for Blackmagic SDI Camera Control protocol
	for (uint8_t index = 0; index < len; index += width) {
		const uint8_t* atem_cc_data_buf = &atem->cmd_payload_buf[CC_ATEM_DATA_OFFSET + index];
		uint8_t* sdi_data_buf = &sdi_buf[CC_HEADER_LEN + CC_CMD_HEADER_LEN + index];
		for (uint8_t offset = 0; offset < width; offset++) {
			sdi_data_buf[offset] = atem_cc_data_buf[width - offset - 1];
		}
	}

	// Updates translated pointer and length padded to 32 bit boundary
	atem->cmd_payload_len = CC_HEADER_LEN + CC_CMD_HEADER_LEN + ((len + 3) & ~3);
	atem->cmd_payload_buf = sdi_buf;

	// Clears padding bytes
	for (uint16_t i = CC_HEADER_LEN + CC_CMD_HEADER_LEN + len; i < atem->cmd_payload_len; i++) {
		atem->cmd_payload_buf[i] = 0x00;
	}
}
