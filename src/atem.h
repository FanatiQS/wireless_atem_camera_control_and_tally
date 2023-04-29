#include <stdbool.h>
#include <stdint.h>

// Include guard
#ifndef ATEM_H
#define ATEM_H

// The Blackmagic bluetooth UUIDs
#define ATEM_BLE_UUID_SERVICE "291D567A-6D75-11E6-8B77-86F30CA893D3"
#define ATEM_BLE_UUID_CHARACTERISTIC "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB"

// Commands in 32 bit integer format for easier comparison
#define ATEM_CMDNAME(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | (d))
#define ATEM_CMDNAME_CAMERACONTROL ATEM_CMDNAME('C', 'C', 'd', 'P')
#define ATEM_CMDNAME_TALLY ATEM_CMDNAME('T', 'l', 'I', 'n')
#define ATEM_CMDNAME_VERSION ATEM_CMDNAME('_', 'v', 'e', 'r')

// ATEM states available as return value from atem_parse
enum atem_status_t {
	ATEM_STATUS_ERROR = -1,
	ATEM_STATUS_WRITE = 0,
	ATEM_STATUS_ACCEPTED = 0x02,
	ATEM_STATUS_REJECTED = 0x03,
	ATEM_STATUS_CLOSING = 0x04,
	ATEM_STATUS_CLOSED = 0x05,
	ATEM_STATUS_WRITE_ONLY = 6,
	ATEM_STATUS_NONE = 7
};

// Default port for ATEM
#define ATEM_PORT 9910

// Number of seconds before switcher kills the connection for no acknowledge sent
#define ATEM_TIMEOUT 5

// Size of read buffer in an atem_t struct
#define ATEM_MAX_PACKET_LEN 2047

// Contains incoming and outgoing ATEM socket data
typedef struct atem_t {
	uint8_t *writeBuf;
	uint8_t *cmdBuf;
	uint16_t dest;
	uint16_t readLen;
	uint16_t writeLen;
	uint16_t cmdLen;
	uint16_t cmdIndex;
	uint16_t lastRemoteId;
	uint8_t readBuf[ATEM_MAX_PACKET_LEN];
	bool pvwTally;
	bool pgmTally;
} atem_t;

// Makes functions available in C++
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Restarts the connection when disconnected or not yet connected.
 *
 * @details Sets the atem.writeBuf to request a new session on the switcher
 * using a SYN packet that has to be manually sent to the switcher.
 * Calling atem_parse on the response from the switcher will return either
 * ATEM_CONNECTION_OK or ATEM_CONNECTION_REJECTED.
 * If connection times out before receiving a response, the connection can be
 * restarted normally.
 *
 * @attention If the atem connection context already has an active connection,
 * resetting and sending a SYN packet would open a new session without first
 * closing the existing one and causing major issues for both connections.
 * Close any existing connection before resetting, or reset as a response
 * to the prior connection being dropped.
 *
 * @param[in,out] atem The atem connection context to reset the connection for.
 */
void atem_connection_reset(struct atem_t *atem);

/**
 * @brief Requests the connection to close.
 *
 * @details Sets the atem.writeBuf to a closing request that manually has to
 * be sent to the switcher.
 * Content of any future packets will be ignored by atem_parse.
 * Write buffer remains as a close request until receiving an
 * ATEM_CONNECTION_CLOSED opcode and should continue to be sent for every packet
 * in case the first request packet was dropped.
 * When receiving ATEM_CONNECTION_CLOSED opcode, the write buffer can be sent
 * to automatically restart the connection or ignored to completely close it.
 *
 * @param[in,out] atem The atem connection context to close the connection for.
 */
void atem_connection_close(struct atem_t *atem);

/**
 * @brief Parses the data available in atem.readBuf.
 *
 * Set content of atem.readBuf manually by receiving UDP data to it with a max
 * length of ATEM_MAX_PACKET_LEN.
 * This function MUST be called before processing commands with
 * atem_cmd_next.
 *
 * @param[in,out] atem The atem connection context containing the data to parse.
 * @returns The connection state of the context to the switcher after
 * processing the packet (one of the ATEM_CONNECTION_* macros).
 *
 * @note It is not required to use the return value from this function.
 */
enum atem_status_t atem_parse(struct atem_t *atem);

uint32_t atem_cmd_next(struct atem_t *atem);

//!! This function has to be called before atem_tally_translate
bool atem_tally_updated(struct atem_t *atem);

void atem_tally_translate(struct atem_t *atem);

void atem_cc_translate(struct atem_t *atem);

// Ends extern C block
#ifdef __cplusplus
}
#endif

/**
 * @brief Checks if there are any commands available to process.
 *
 * @details Use this function in a while loop to process all commands in an
 * ATEM packet after parsing it with atem_parse.
 * Get ATEM commands with atem_cmd_next if there are commands available.
 *
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Boolean indicating if there are commands available to process.
 */
#define atem_cmd_available(atem) ((atem)->cmdIndex < (atem)->readLen)

// Gets the major and minor version of the protocol for a version command
#define atem_protocol_major(atem) ((atem)->cmdBuf[0] << 8 | (atem)->cmdBuf[1])
#define atem_protocol_minor(atem) ((atem)->cmdBuf[2] << 8 | (atem)->cmdBuf[3])

// Gets the camera destination number for a camera control command
#define atem_cc_dest(atem) ((atem)->cmdBuf[0])

// Ends include guard
#endif
