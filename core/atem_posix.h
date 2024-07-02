// Include guard
#ifndef ATEM_POSIX_H
#define ATEM_POSIX_H

#include <stdbool.h> // bool
#include <stdint.h> // uint32_t

#include <netinet/in.h> // in_addr_t

#include "./atem.h" // struct atem



/**
 * @brief Status codes returnd from \ref atem_poll.
 */
enum atem_posix_status {
	/**
	 * A network error occured in either `atem_send` or `atem_recv`. `errno` will be set unless `atem_recv` got
	 * a packet that was shorter than minimum size for an ATEM packet.
	 */
	ATEM_POSIX_STATUS_ERROR_NETWORK = -2,
	ATEM_POSIX_STATUS_ERROR_PARSE = ATEM_STATUS_ERROR,
	ATEM_POSIX_STATUS_WRITE = ATEM_STATUS_WRITE,
	ATEM_POSIX_STATUS_ACCEPTED = ATEM_STATUS_ACCEPTED,
	ATEM_POSIX_STATUS_REJECTED = ATEM_STATUS_REJECTED,
	ATEM_POSIX_STATUS_CLOSING = ATEM_STATUS_CLOSING,
	ATEM_POSIX_STATUS_CLOSED = ATEM_STATUS_CLOSED,
	ATEM_POSIX_STATUS_WRITE_ONLY = ATEM_STATUS_WRITE_ONLY,
	ATEM_POSIX_STATUS_NONE = ATEM_STATUS_NONE,
	/**
	 * The connection to the ATEM server was dropped.
	 * This status will continuously be reported until the connection is re-established.
	 * Call `atem_send` to automatically reconnect.
	 * It is recommended to wait a few hundred milliseconds before attempting a reconnect to not harass the ATEM.
	 */
	ATEM_POSIX_STATUS_DROPPED
};

// Makes functions available to C++ with extern C block
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates UDP socket for communicating with ATEM server
 * @param addr IP address of ATEM server to connect to
 * @param atem Blank ATEM context to connect to use with the created socket
 * @return UDP socket connected to ATEM server
 */
int atem_init(in_addr_t addr, struct atem* atem);

/**
 * @brief Sends cached ATEM UDP packet from ATEM context
 * @param sock ATEM servers UDP socket to send the data through
 * @param atem ATEM context containing the data to send
 * @return Indicates if sending data was successful or not, `errno` is set on failure
 */
bool atem_send(int sock, struct atem* atem);

/**
 * @brief Reads next UDP packet into ATEM context
 * @param sock ATEM servers UDP socket to read data from
 * @param atem ATEM context to read data into
 * @return Indicates if packet was read successful or not, `errno` is set unless received packet was too short
 */
bool atem_recv(int sock, struct atem* atem);

/**
 * @brief Receives and parses ATEM packets
 * @param sock ATEM server UDP socket to read data from
 * @param atem ATEM context to read data into
 * @return Status code describing the result from reading and parsing ATEM packet
 */
enum atem_posix_status atem_poll(int sock, struct atem* atem);

/**
 * @brief Reads ATEM packets and returns its commands
 * @param sock ATEM server UDP socket to read data from
 * @param atem ATEM context to read data into
 * @return Command in an ATEM packet or -1 on error from `atem_poll`
 */
uint32_t atem_next(int sock, struct atem* atem);

#ifdef __cplusplus
}
#endif

#endif // ATEM_POSIX_H
