#include <stdbool.h> // bool, true, false
#include <errno.h> // errno, EFAULT
#include <assert.h> // assert
#include <stdint.h> // uint32_t
#include <string.h> // memset

#include <sys/socket.h> // socket, AF_INET, SOCK_DGRAM, connect, recv, send, struct sockaddr
#include <netinet/in.h> // in_addr_t, struct sockaddr_in
#include <arpa/inet.h> // htons
#include <poll.h> // poll, struct pollfd, POLLIN
#include <unistd.h> // close, sleep
#include <sys/types.h> // ssize_t

#include "./atem.h" // struct atem, ATEM_PORT, atem_connection_reset, ATEM_TIMEOUT_MS, ATEM_PACKET_LEN_MAX, enum atem_status, atem_parse, ATEM_STATUS_WRITE, atem_cmd_available
#include "./atem_protocol.h" // ATEM_LEN_HEADER
#include "./atem_posix.h" // enum atem_posix_status, ATEM_POSIX_STATUS_ERROR_NETWORK, ATEM_POSIX_STATUS_DROPPED

// Initializes ATEM communication by creating UDP socket for context
bool atem_init(struct atem_posix_ctx* atem_ctx, in_addr_t addr) {
	assert(atem_ctx != NULL);

	// Sets errno to indicate bad address if ATEM address is not valid
	if (addr == (in_addr_t)-1) {
		errno = EFAULT;
		return false;
	}

	// Creates UDP socket for ATEM server connection
	atem_ctx->sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (atem_ctx->sock == -1) {
		return false;
	}

	// Connects UDP socket to only communicate with ATEM server
	struct sockaddr_in sockaddr = {
		.sin_family = AF_INET,
		.sin_port = htons(ATEM_PORT),
		.sin_addr.s_addr = addr
	};
	if (connect(atem_ctx->sock, (const struct sockaddr*)&sockaddr, sizeof(sockaddr))) {
		close(atem_ctx->sock);
		return false;
	}

	// Initializes context and starts opening handshake
	atem_ctx->atem.tally_pgm = 0;
	atem_ctx->atem.tally_pvw = 0;
	atem_connection_reset(&atem_ctx->atem);

	return true;
}

// Sens buffered packet in context using ATEM UDP socket
bool atem_send(struct atem_posix_ctx* atem_ctx) {
	assert(atem_ctx != NULL);

	struct atem* atem = &atem_ctx->atem;
	assert(atem->write_buf != NULL);
	assert(atem->write_len > 0);
	assert(((atem->write_buf[0] << 8 | atem->write_buf[1]) & ATEM_PACKET_LEN_MAX) == atem->write_len);
	ssize_t sent = send(atem_ctx->sock, atem->write_buf, atem->write_len, 0);
	assert(sent == -1 || sent == atem->write_len);
	return sent == atem->write_len;
}

// Reads next ATEM packet from server
bool atem_recv(struct atem_posix_ctx* atem_ctx) {
	assert(atem_ctx != NULL);
	errno = 0;
	return recv(atem_ctx->sock, atem_ctx->atem.read_buf, ATEM_PACKET_LEN_MAX, 0) >= ATEM_LEN_HEADER;
}

// Reads and parses ATEM packets until a packet with payload is received
enum atem_posix_status atem_poll(struct atem_posix_ctx* atem_ctx) {
	assert(atem_ctx != NULL);

	struct pollfd poll_fd = { .fd = atem_ctx->sock, .events = POLLIN };
	int poll_len = poll(&poll_fd, 1, ATEM_TIMEOUT_MS);

	if (poll_len != 1) {
		// Returns network error status if poll gets an error
		if (poll_len == -1) {
			return ATEM_POSIX_STATUS_ERROR_NETWORK;
		}

		// Resets to send new opening handshake if connection is dropped
		assert(poll_len == 0);
		atem_connection_reset(&atem_ctx->atem);
		return ATEM_POSIX_STATUS_DROPPED;
	}

	if (!atem_recv(atem_ctx)) {
		return ATEM_POSIX_STATUS_ERROR_NETWORK;
	}

	// Parses and acknowledges received ATEM packet
	enum atem_status status = atem_parse(&atem_ctx->atem);
	if (!(status & 1) && !atem_send(atem_ctx)) {
		return ATEM_POSIX_STATUS_ERROR_NETWORK;
	}

	return (enum atem_posix_status)status;
}

// Returns status codes or iterates through commands in ATEM packets
uint32_t atem_next(struct atem_posix_ctx* atem_ctx) {
	assert(atem_ctx != NULL);
	while (!atem_cmd_available(&atem_ctx->atem)) {
		enum atem_posix_status status;
		while ((status = atem_poll(atem_ctx)) != ATEM_POSIX_STATUS_WRITE) {
			switch (status) {
				case ATEM_POSIX_STATUS_NONE:
				case ATEM_POSIX_STATUS_WRITE_ONLY: {
					break;
				}
				case ATEM_POSIX_STATUS_DROPPED: {
					atem_send(atem_ctx);
					return ATEM_POSIX_STATUS_DROPPED;
				}
				default: {
					return (uint32_t)status;
				}
			}
		}
	}
	return atem_cmd_next(&atem_ctx->atem);
}
