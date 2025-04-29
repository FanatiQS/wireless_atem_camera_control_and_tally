/**
 * @file
 * @brief Low level API for communicating with an ATEM server
 */

// Include guard
#ifndef ATEM_H
#define ATEM_H

#include <stdbool.h> // bool
#include <stdint.h> // uint8_t, uint16_t

/** Bluetooth service UUID for communicating with blackmagic cameras */
#define ATEM_BLE_UUID_SERVICE "291D567A-6D75-11E6-8B77-86F30CA893D3"

/** Bluetooth characteristic UUID for sending camera control data to blackmagic cameras */
#define ATEM_BLE_UUID_CHARACTERISTIC "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB"

// Gets thread local implementation
#if !defined(ATEM_THREAD_LOCAL)
// C23 has native support with keyword
#if __STDC_VERSION__ >= 202311
#define ATEM_THREAD_LOCAL thread_local
// C11 has support with wrapper in <thread.h> header that doesn't have to be supported
#elif __STDC_VERSION__ >= 201112
#define ATEM_THREAD_LOCAL _Thread_local
// No thread safe support
#else
#define NO_ATEM_THREAD_LOCAL
#define ATEM_THREAD_LOCAL
#endif
#endif // !defined(ATEM_THREAD_LOCAL)

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
 * Maximum size of an ATEM packet, used by @ref atem.read_buf.
 */
#define ATEM_PACKET_LEN_MAX 2047

/**
 * Converts a command name from 4 characters to a 32bit integer
 */
#define ATEM_CMDNAME(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | (d))

/**
 * ATEM packet commands returned from atem_cmd_next(). Consists of 4
 * ascii characters but are here represented as a single 32bit
 * integer for fast comparisons and makes them usable in switch statements.
 */
enum atem_commands {
	/**
	 * Contains the ATEM protocol version number. Can be extracted with
	 * atem_protocol_major() and atem_protocol_minor().
	 */
	ATEM_CMDNAME_VERSION = ATEM_CMDNAME('_', 'v', 'e', 'r'),
	/**
	 * Contains tally states for all inputs. Use atem_tally_updated() to
	 * store tally status for camera identifier @ref atem.dest in
	 * @ref atem.
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
enum atem_status {
	/**
	 * Unable to parse the packet. This packet should be ignored.
	 */
	ATEM_STATUS_ERROR = -1,
	/**
	 * Received data that has to be acknowledged by sending the
	 * @ref atem.write_buf. It might also contain commands to process
	 * with atem_cmd_next().
	 */
	ATEM_STATUS_WRITE = 0,
	/**
	 * Client has established a connection to the ATEM switcher and has
	 * to acknowledge the packet by sending the @ref atem.write_buf.
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
	 * should to be responded to by sending the @ref atem.write_buf.
	 */
	ATEM_STATUS_CLOSING = 0x04,
	/**
	 * Client has been successfully disconnected from the ATEM switcher
	 * and no further action is needed. Reconnecting can be done right
	 * away.
	 */
	ATEM_STATUS_CLOSED = 0x05,
	/**
	 * Received data that has to be acknowledged by sending the
	 * @ref atem.write_buf. The commands in the payload should NOT
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

/**
 * Contains incoming and outgoing ATEM socket data.
 */
typedef struct atem {
	uint8_t* write_buf;
	uint8_t* cmd_buf;
	uint16_t read_len;
	uint16_t write_len;
	uint16_t cmd_len;
	uint16_t cmd_index_next;
	uint16_t remote_id_last;
	uint8_t dest;
	uint8_t read_buf[ATEM_PACKET_LEN_MAX];
	bool tally_pvw;
	bool tally_pgm;
} atem_t;

// Makes functions available in C++
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Restarts the connection when disconnected or not yet connected.
 *
 * Sets the @ref atem.write_buf to a request that opens a new session
 * that manually has to be sent to the switcher. All future data received from
 * the switcher has to be parsed with atem_parse(). The handshake is completed
 * when atem_parse() returns @ref ATEM_STATUS_ACCEPTED or
 * @ref ATEM_STATUS_REJECTED. If connection times out before receiving a
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
void atem_connection_reset(struct atem* atem);

/**
 * @brief Requests the ATEM connection to close.
 *
 * Sets the @ref atem.write_buf to a closing request that manually has to
 * be sent to the switcher. Commands in any future packets will be ignored
 * by atem_parse(). Write buffer remains as a close request until receiving an
 * @ref ATEM_STATUS_CLOSED and should continue to be sent for every packet
 * in case the first close request packet was dropped. The connection is
 * completely closed when receiving the @ref ATEM_STATUS_CLOSED, a new
 * connection can be restarted by calling atem_connection_reset() again.
 *
 * @param[in,out] atem The atem connection context to close the connection for.
 */
void atem_connection_close(struct atem* atem);

/**
 * @brief Parses the ATEM packet available in @ref atem.read_buf.
 *
 * If the status received from parsing an ATEM packet is @ref ATEM_STATUS_WRITE,
 * it might contain commands that can be processed using atem_cmd_next().
 * The statuses @ref ATEM_STATUS_WRITE, @ref ATEM_STATUS_WRITE_ONLY,
 * @ref ATEM_STATUS_ACCEPTED and @ref ATEM_STATUS_CLOSING all require sending
 * a response that is put in @ref atem.write_buf.
 * These statues are all represented by even numbers, so determining if a status
 * requires sending data or not is as easy as checking if the status is odd or
 * even.
 * 
 * @attention Copy ATEM UDP packet to @ref atem.read_buf array manually before
 * calling this function with a max length of @ref ATEM_PACKET_LEN_MAX.
 *
 * @param[in,out] atem The atem connection context containing the data to parse.
 * @returns Describes the basic purpose of the received ATEM packet.
 */
enum atem_status atem_parse(struct atem* atem);

/**
 * @brief Checks if there are any commands available to process.
 *
 * Use this function to loop through all commands in an ATEM packet if
 * atem_parse() returns @ref ATEM_STATUS_WRITE. For each command, get its name
 * using atem_cmd_next().
 * 
 * @attention This function can ONLY be called after atem_parse() returns
 * @ref ATEM_STATUS_WRITE.
 *
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Indicates if there are commands available to process.
 */
static inline bool atem_cmd_available(struct atem* atem) {
	return (atem->cmd_index_next < atem->read_len);
}

/**
 * @brief Gets the next command name from the ATEM packet.
 * 
 * If atem_parse() returns status code @ref ATEM_STATUS_WRITE, this function can
 * be used in combination with atem_cmd_available() to iterate through all
 * commands in the ATEM packet.
 * Some commands are available in @ref atem_commands.
 * Command names can be constructed using ATEM_CMDNAME() macro function.
 * 
 * @attention This function can ONLY be called when atem_parse() returns
 * @ref ATEM_STATUS_WRITE.
 * @attention This function should ONLY be called when atem_cmd_available()
 * returns true to indicate there is a command available in the ATEM packet.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns A command name as a 32 bit integer.
 */
uint32_t atem_cmd_next(struct atem* atem);

/**
 * @brief Gets the major version of the ATEM protocol from @ref ATEM_CMDNAME_VERSION
 * command in ATEM protocol.
 * 
 * @attention This function can only be called after atem_parse() returns
 * @ref ATEM_CMDNAME_VERSION.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns The major version of the ATEM protocol.
 */
static inline uint16_t atem_protocol_major(struct atem* atem) {
	return (atem->cmd_buf[0] << 8 | atem->cmd_buf[1]) & 0xffff;
}

/**
 * @brief Gets the minor version of the ATEM protocol from @ref ATEM_CMDNAME_VERSION
 * command in ATEM protocol.
 * 
 * @attention This function can only be called after atem_parse() returns
 * @ref ATEM_CMDNAME_VERSION.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns The minor version of the ATEM protocol.
 */
static inline uint16_t atem_protocol_minor(struct atem* atem) {
	return (atem->cmd_buf[2] << 8 | atem->cmd_buf[3]) & 0xffff;
}

/**
 * @brief Gets tally status from @ref ATEM_CMDNAME_TALLY command in ATEM packet.
 * 
 * Call this function when receiving a @ref ATEM_CMDNAME_TALLY command to update
 * the values at @ref atem.tally_pvw and @ref atem.tally_pgm based on the
 * camera identifier in @ref atem.dest.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name @ref ATEM_CMDNAME_TALLY.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Indicates if the internal tally state changed.
 */
bool atem_tally_updated(struct atem* atem);

/**
 * @brief Checks dest in the @ref ATEM_CMDNAME_CAMERACONTROL command in ATEM packet.
 * 
 * Call this function when receiving a @ref ATEM_CMDNAME_CAMERACONTROL
 * command to check if the content of the command is for the contexts camera
 * identifier defined in @ref atem.dest.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name @ref ATEM_CMDNAME_CAMERACONTROL.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 * @returns Indicates if the internal camera identifier matched the
 * commands destination identifier.
 */
static inline bool atem_cc_updated(struct atem* atem) {
	return (atem->cmd_buf[0] == atem->dest);
}

/**
 * @brief Translates camera control data for @ref ATEM_CMDNAME_CAMERACONTROL command
 * in ATEM packet.
 * 
 * Translates ATEM camera control protocol to Blackmagic SDI Camera Control
 * Protocol. The translated data is available in the @ref atem.cmd_buf and the
 * length of the data block in @ref atem.cmd_len.
 * 
 * @attention This function can ONLY be called when atem_cmd_next() returns
 * the command name @ref ATEM_CMDNAME_CAMERACONTROL.
 * @attention The transformed data is still located inside the @ref atem.read_buf
 * and will therefore be overwritten when parsing next packet.
 * 
 * @param[in,out] atem The atem connection context containing the parsed data.
 */
void atem_cc_translate(struct atem* atem);

// Ends extern C block
#ifdef __cplusplus
}
#endif

#endif // ATEM_H
