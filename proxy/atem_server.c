#include <stdlib.h> // malloc, free, abort
#include <assert.h> // assert
#include <stdbool.h> // bool, false, true
#include <stddef.h> // NULL
#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <string.h> // memset
#include <stdint.h> // uint8_t, uint16_t, int16_t

#include <sys/socket.h> // socket, AF_INET, SOCK_DGRAM, bind, struct sockaddr
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons, INADDR_ANY
#include <unistd.h> // close

#include "./atem_debug.h" // DEBUG_PRINTF
#include "./atem_server.h" // struct atem_server, atem_session_get
#include "./atem_session.h" // struct atem_session, atem_session_send
#include "./atem_packet.h" // struct atem_packet, atem_packet_create, struct atem_packet_session
#include "../core/atem.h" // ATEM_PORT
#include "../core/atem_protocol.h" // ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, ATEM_RESEND_TIME, ATEM_PORT

// Initializes server context with default configuration
struct atem_server atem_server = {
	.sessions_size = 5,
	.sessions_limit = 5, // @todo create macro in atem_protocol.h for this value and use in tests
	.retransmit_delay = ATEM_RESEND_TIME,
	.ping_interval = ATEM_PING_INTERVAL
};

// Releases all resources server has allocated
void atem_server_release(void) {
	assert(atem_server.packet_queue_head == NULL);
	assert(atem_server.sessions_connected == 0);
	assert(atem_server.sessions_len == 0);

	// Releases UDP socket
	int close_err = close(atem_server.sock);
	if (close_err != 0) {
		assert(close_err == -1);
		perror("Error during closing of ATEM servers UDP socket");
	}

	// Releases session array
	assert(atem_server.sessions != NULL);
	free(atem_server.sessions);
	atem_server.sessions = NULL;

	assert(atem_server.closing == true);
	atem_server.closing = false;
}



/**
 * Initializes ATEM proxy server
 * @public
 * @return Indicates if initialization was successful or not and sets `errno` with no allocations being made on failure
 */
bool atem_server_init(void) {
	assert(atem_server.packet_queue_head == NULL);
	assert(atem_server.packet_queue_tail == NULL);
	assert(atem_server.sessions_connected == 0);
	assert(atem_server.sessions_len == 0);
	assert(atem_server.sessions_size > 1);
	assert(atem_server.sessions_limit > 1);
	assert(atem_server.retransmit_delay < atem_server.ping_interval);

	// Creates UDP socket for ATEM server
	atem_server.sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (atem_server.sock == -1) {
		return false;
	}

	// Listens for any ip address on ATEM UDP server port
	struct sockaddr_in server_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = INADDR_ANY
	};
	if (bind(atem_server.sock, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
		close(atem_server.sock);
		return false;
	}

	// Allocates sessions array for connections to be placed into
	assert(atem_server.sessions == NULL);
	atem_server.sessions = malloc(sizeof(struct atem_session) * atem_server.sessions_size);
	if (atem_server.sessions == NULL) {
		close(atem_server.sock);
		return false;
	}

	return true;
}

// @todo
void atem_server_recv(void) {
	DEBUG_PRINTF("Receiving ATEM data\n");

	// Reads ATEM client packet from servers UDP socket
	uint8_t buf[ATEM_PACKET_LEN_MAX];
	struct sockaddr_in peer_addr;
	socklen_t peer_addr_len = sizeof(peer_addr);
	ssize_t recved = recvfrom(
		atem_server.sock,
		buf, ATEM_PACKET_LEN_MAX,
		0,
		(struct sockaddr*)&peer_addr, &peer_addr_len
	);
	assert(peer_addr_len == sizeof(peer_addr));
	if (recved == -1) {
		perror("Failed to read proxy data");
		return;
	}
	assert(recved >= 0);
	if (recved > ATEM_PACKET_LEN_MAX) {
		DEBUG_PRINTF("UDP packet was too big\n");
		return;
	}
	if (recved < ATEM_LEN_HEADER) {
		DEBUG_PRINTF("UDP packet as too small\n");
		return;
	}
	uint16_t len = recved & 0xffff;

	DEBUG_PRINTF("Received: ");
	DEBUG_PRINT_BUF(buf, len);

	// @todo
	if (!(buf[ATEM_INDEX_SESSIONID_HIGH] & 0x80)) {
		// @todo
		if (buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN) {
			// Rejects SYN packets with invalid length
			if (len != ATEM_LEN_SYN) {
				DEBUG_PRINTF("Unexpected length for SYN packet: %d\n", len);
				return;
			}

			// Rejects SYN packets with opcodes other than OPEN
			if (buf[ATEM_INDEX_OPCODE] != ATEM_OPCODE_OPEN) {
				DEBUG_PRINTF("Expected open opcode: %d\n", buf[ATEM_INDEX_OPCODE]);
				return;
			}

			// @todo
			atem_session_create(buf[ATEM_INDEX_SESSIONID_HIGH], buf[ATEM_INDEX_SESSIONID_LOW], &peer_addr);
		}
		// @todo
		else if (!(buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_ACK)) {
			DEBUG_PRINTF("Unexpected flags: 0x%02x\n", buf[ATEM_INDEX_FLAGS]);
		}
		// @todo
		else {
			atem_session_connect(buf[ATEM_INDEX_SESSIONID_HIGH], buf[ATEM_INDEX_SESSIONID_LOW], &peer_addr);
		}
		return;
	}

	// @todo
	uint16_t session_id = buf[ATEM_INDEX_SESSIONID_HIGH] << 8 | buf[ATEM_INDEX_SESSIONID_LOW];
	int16_t session_index = atem_session_lookup_get(session_id);
	if (session_index == -1) {
		DEBUG_PRINTF("Session does not exist: 0x%04x\n", session_id);
		return;
	}
	struct atem_session* session = atem_session_get(session_index);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	if (!atem_session_peer_validate(session, &peer_addr)) {
		return;
	}
	uint8_t flags = buf[ATEM_INDEX_FLAGS];

	// @todo acknowledgements
	if (flags & ATEM_FLAG_ACK) {
		uint16_t ack_id = buf[ATEM_INDEX_ACKID_HIGH] << 8 | buf[ATEM_INDEX_ACKID_LOW];
		atem_session_acknowledge(session_index, ack_id);
	}

	// @todo
	if (flags & ATEM_FLAG_SYN) {
		uint8_t opcode = buf[ATEM_INDEX_OPCODE];
		if (opcode == ATEM_OPCODE_CLOSING) {
			atem_session_closing(session_index);
		}
		else if (opcode == ATEM_OPCODE_CLOSED) {
			atem_session_closed(session_index);
		}
	}

	// @todo
	if (flags & ~(ATEM_FLAG_SYN | ATEM_FLAG_ACK | ATEM_FLAG_RETX | ATEM_FLAG_ACKREQ)) {
		printf("Unsupported flags: 0x%02x\n", flags);
	}
}
