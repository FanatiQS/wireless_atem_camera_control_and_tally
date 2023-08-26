// Include guard
#ifndef ATEM_H
#define ATEM_H

#include <stdbool.h>
#include <stdint.h>

// The Blackmagic bluetooth UUIDs
#define ATEM_BLE_UUID_SERVICE "291D567A-6D75-11E6-8B77-86F30CA893D3"
#define ATEM_BLE_UUID_CHARACTERISTIC "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB"

/**
 * Default port for ATEM
 */
#define ATEM_PORT 9910

/**
 * Number of seconds before ATEM switcher kills the connection for not
 * acknowledging packets it sent.
 */
#define ATEM_TIMEOUT 5

/**
 * Number of milliseconds before ATEM switcher kills the connection for not
 * acknowledging packets it sent.
 */
#define ATEM_TIMEOUT_MS (ATEM_TIMEOUT * 1000)

/**
 * Maximum size of an ATEM packet, used by \ref atem_t.readBuf.
 */
#define ATEM_MAX_PACKET_LEN 2047

/**
 * Converts a command name from 4 characters to a 32bit integer
 */
#define ATEM_CMDNAME(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | (d))

/**
 * ATEM packet commands returned from atem_cmd_next(). Consists of 4
 * ascii characters but are here represented as a single 32bit
 * integer for fast comparisons and makes them usable in switch statements.
 */
enum atem_commands_t {
	/**
	 * Contains the ATEM protocol version number. Can be extracted with
	 * atem_protocol_major() and atem_protocol_minor().
	*/
	ATEM_CMDNAME_VERSION = ATEM_CMDNAME('_', 'v', 'e', 'r'),
	/**
	 * Contains tally states for all inputs. Use atem_tally_updated() to
 	 * store tally status for camera identifier \ref atem_t.dest in
	 * \ref atem_t.
	 */
	ATEM_CMDNAME_TALLY = ATEM_CMDNAME('T', 'l', 'I', 'n'),
	/**
	 * Contains camera control data. Can be transformed from ATEM protocol
	 * structure to Blackmagic SDI Camera Control Protocol structure with
	 * atem_cc_translate().
	 */
	ATEM_CMDNAME_CAMERACONTROL = ATEM_CMDNAME('C', 'C', 'd', 'P')
};

/**
 * ATEM parse statues returned from atem_parse().
 */
enum atem_status_t {
	/**
	 * Unable to parse the packet. This packet should be ignored.
	 */
	ATEM_STATUS_ERROR = -1,
	/**
	 * Received data that has to be acknowledged by sending the
	 * \ref atem_t.writeBuf. It might also contain commands to process
	 * with atem_cmd_next().
	 */
	ATEM_STATUS_WRITE = 0,
	/**
	 * Client has established a connection to the ATEM switcher and has
	 * to acknowledge the packet by sending the \ref atem_t.writeBuf.
	 */
	ATEM_STATUS_ACCEPTED = 0x02,
	/**
	 * The ATEM switcher was unable to adopt the client, no further
	 * action is required. Retrying can be done as soon as possible
	 * without overwhelming the switcher.
	 */
	ATEM_STATUS_REJECTED = 0x03,
	/**
	 * The ATEM switcher requests closing of the session. This action
	 * should to be responded to by sending the \ref atem_t.writeBuf.
	 */
	ATEM_STATUS_CLOSING = 0x04,
	/**
	 * Client has been successfully disconnected from the ATEM swithcer
	 * and no further action is needed. Reconnecting can be done right
	 * away.
	 */
	ATEM_STATUS_CLOSED = 0x05,
	/**
	 * Received data that has to be acknowledged by sending the
	 * \ref atem_t.writeBuf. The commands in the payload should NOT
	 * be processed.
	 */
	ATEM_STATUS_WRITE_ONLY = 6,
	/**
	 * The ATEM packet did not contain any data important to this parser.
	 * This should only occur if requests have been sent to the ATEM
	 * switcher and it sends back an acknowledgement.
	 */
	ATEM_STATUS_NONE = 7
};

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
 * Sets the \ref atem_t.writeBuf to a request that opens a new session
 * that manually has to be sent to the switcher. All future data received from
 * the switcher has to be parsed with atem_parse(). The handshake is completed
 * when atem_parse() returns \ref ATEM_STATUS_ACCEPTED or
 * \ref ATEM_STATUS_REJECTED. If connection times out before receiving a
 * response, the connection process can be restarted.
 *
 * @attention If the atem connection context already has an active connection,
 * resetting and sending a SYN packet would open a new session without first
 * closing the existing one, causing major issues for both connections.
 * Close any existing connection before resetting, or reset as a response
 * to the prior connection being dropped.
 *
 * @param[in,out] atem The atem connection context to reset the connection for.
 */
void atem_connection_reset(struct atem_t *atem);

/**
 * @brief Requests the ATEM connection to close.
 *
 * Sets the \ref atem_t.writeBuf to a closing request that manually has to
 * be sent to the switcher. Commands in any future packets will be ignored
 * by atem_parse(). Write buffer remains as a close request until receiving an
 * \ref ATEM_STATUS_CLOSED and should continue to be sent for every packet
 * in case the first close request packet was dropped. The connection is
 * completely closed when receiving the \ref ATEM_STATUS_CLOSED, a new
 * connection can be restarted by calling atem_connection_reset() again.
 *
 * @param[in,out] atem The atem connection context to close the connection for.
 */
void atem_connection_close(struct atem_t *atem);

/**
 * @brief Parses the ATEM packet available in \ref atem_t.readBuf.
 *
 * If the status received from parsing an ATEM packet is \ref ATEM_STATUS_WRITE,
 * it might contain commands that can be processed using atem_cmd_next().
 * The statuses \ref ATEM_STATUS_WRITE, \ref ATEM_STATUS_WRITE_ONLY,
 * \ref ATEM_STATUS_ACCEPTED and \ref ATEM_STATUS_CLOSING all require sending
 * a response that is put in \ref atem_t.writeBuf.
 * These statues are all represented by even numbers, so detemining if a status
 * requires sending data or not is as easy as checking if the status is odd or
 * even.
 * 
 * @attention Copy ATEM UDP packet to \ref atem_t.readBuf array manually before
 * calling this function with a max length of \ref ATEM_MAX_PACKET_LEN.
 *
 * @param[in,out] atem The atem connection context containing the data to parse.
 * @returns Describes the basic purpous of the received ATEM packet.
 */
enum atem_status_t atem_parse(struct atem_t *atem);

/**
 * @brief Gets the next command name from the ATEM packet.
 * 
 * If atem_parse() returns status code \ref ATEM_STATUS_WRITE, this function can
 * be used in combination with atem_cmd_available() to iterate through all
 * commands in ATEM the packet.
 * Some commands are available in \ref atem_commands_t.
 * Command names can be constructed using ATEM_CMDNAME() macro function.
 * 
 * @attention This function can ONLY be called when atem_parse() returns
 * \ref ATEM_STATUS_WRITE.
 * @attention This function should ONLY be called when atem_cmd_available()
 * returns true to indicate there is a command available in the ATEM packet.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns A command name as a 32 bit integer.
 */
uint32_t atem_cmd_next(struct atem_t *atem);

/**
 * @brief Gets tally status from \ref ATEM_CMDNAME_TALLY command in ATEM packet.
 * 
 * Call this function when receiving a \ref ATEM_CMDNAME_TALLY command to update
 * the values at \ref atem_t.pvwTally and \ref atem_t.pgmTally based on the
 * camera idenfifier in \ref atem_t.dest.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name \ref ATEM_CMDNAME_TALLY.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Indicates if the internal tally state changed.
 */
bool atem_tally_updated(struct atem_t *atem);

/**
 * @brief Translates camera control data for \ref ATEM_CMDNAME_CAMERACONTROL command
 * in ATEM packet.
 * 
 * Translates ATEM camera control protocol to Blackmagic SDI Camera Control
 * Protocol. The translated data is available in the \ref atem_t.cmdBuf and the
 * length of the data block in \ref atem_t.cmdLen.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name \ref ATEM_CMDNAME_CAMERACONTROL.
 * @attention The transformed data is still located inside the \ref atem_t.readBuf
 * and will therefore be overwritten when parsing next packet.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 */
void atem_cc_translate(struct atem_t *atem);

// Ends extern C block
#ifdef __cplusplus
}
#endif

/**
 * @brief Checks if there are any commands available to process.
 *
 * Use this function to loop through all commands in an ATEM packet if
 * atem_parse() returns \ref ATEM_STATUS_WRITE. For each command, get its name
 * using atem_cmd_next().
 * 
 * @attention This function can ONLY be called after atem_parse() returns
 * \ref ATEM_STATUS_WRITE.
 *
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Boolean indicating if there are commands available to process.
 */
#define atem_cmd_available(atem) ((atem)->cmdIndex < (atem)->readLen)

/**
 * Gets the major version of the ATEM protocol from \ref ATEM_CMDNAME_VERSION
 * command in ATEM protocol.
 * 
 * @attention This function can only be called after atem_parse() returns
 * \ref ATEM_STATUS_VERSION.
 * @returns The major version of the ATEM protocol.
 */
#define atem_protocol_major(atem) ((atem)->cmdBuf[0] << 8 | (atem)->cmdBuf[1])

/**
 * Gets the minor version of the ATEM protocol from \ref ATEM_CMDNAME_VERSION
 * command in ATEM protocol.
 * 
 * @attention This function can only be called after atem_parse() returns
 * \ref ATEM_STATUS_VERSION.
 * @returns The minor version of the ATEM protocol.
 */
#define atem_protocol_minor(atem) ((atem)->cmdBuf[2] << 8 | (atem)->cmdBuf[3])

/**
 * @brief Checks dest in the \ref ATEM_CMDNAME_CAMERACONTROL command in ATEM packet.
 * 
 * Call this function when receiving a \ref ATEM_CMDNAME_CAMERACONTROL
 * command to check if the content of the command is for the contexts camera
 * identifier defined in \ref atem_t.dest.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name \ref ATEM_CMDNAME_CAMERACONTROL.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Boolean indicating if the internal camera identifier matched the
 * commands destination identifier.
 */
#define atem_cc_updated(atem) ((atem)->cmdBuf[0] == (atem)->dest)

// Ends include guard
#endif
