#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <assert.h> // assert
#include <stdint.h> // uint32_t

#include "./atem_server.h" // atem_server
#include "./atem_packet.h" // atem_packet_broadcast_ping
#include "./atem_debug.h" // DEBUG_PRINTF
#include "./timeout.h"

// Indexes for all timeouts that can be set
enum timeout_index {
	TIMEOUT_INDEX_RETRANSMIT,
	TIMEOUT_INDEX_PING
};

// Index for what type of timeout was returned last
static enum timeout_index timeout_index;

// Gets remaining time between current time and specified timeout
static uint32_t timeout_remaining(struct timespec* now, struct timespec* timeout, uint16_t delay) {
	assert(now != NULL);
	assert(timeout != NULL);

	uint32_t delta = (now->tv_sec - timeout->tv_sec) * 1000 + (now->tv_nsec - timeout->tv_nsec) / 1000000;
	if (delta > delay) return 0;
	return (delay - delta);
}

// Gets time in milliseconds to next timeout
int timeout_get(void) {
	struct timespec now;
	int timespec_result = timespec_get(&now, TIME_UTC);
	assert(timespec_result == TIME_UTC);
	uint32_t timeout = ~0;

	// Gets remaining time for next retransmit packet timeout
	if (atem_server.packet_queue_head != NULL) {
		uint32_t timeout_retx;
		// while (1) {
			timeout_retx = timeout_remaining(
				&now,
				&atem_server.packet_queue_head->timeout,
				atem_server.retransmit_delay
			);
		// 	if (timeout_retx > 0) break;
		// 	atem_packet_retransmit();
		// }
		if (timeout_retx < timeout) {
			assert(timeout_retx >= 0);
			DEBUG_PRINTF("Timeout retransmit: %dms\n", timeout_retx);
			timeout = timeout_retx;
			timeout_index = TIMEOUT_INDEX_RETRANSMIT;
		}
	}

	// Gets remaining time for next ping
	if (atem_server.sessions_connected > 0) {
		uint32_t timeout_ping = timeout_remaining(
			&now,
			&atem_server.ping_timeout,
			atem_server.ping_interval
		);
		if (timeout_ping == 0) {
			atem_packet_broadcast_ping();
		}
		if (timeout_ping < timeout) {
			assert(timeout_ping >= 0);
			DEBUG_PRINTF("Timeout ping: %dms\n", timeout_ping);
			timeout = timeout_ping;
			timeout_index = TIMEOUT_INDEX_PING;
		}
	}

	if (timeout == ~0u) {
		DEBUG_PRINTF("No timeout\n\n");
	}
	else {
		DEBUG_PRINTF("Timeout: %dms\n\n", timeout);
	}

	return timeout;
}

// @todo
struct timespec* timeout_next(void) {
	if (timeout_get() == -1) {
		return NULL;
	}
	switch (timeout_index) {
		case TIMEOUT_INDEX_RETRANSMIT: return &atem_server.packet_queue_head->timeout;
		case TIMEOUT_INDEX_PING: return &atem_server.ping_timeout;
	}
}

// Handles elapsed timeout based on last call to timeout_get
void timeout_dispatch(void) {
	switch (timeout_index) {
		case TIMEOUT_INDEX_RETRANSMIT: {
			atem_packet_retransmit();
			break;
		}
		case TIMEOUT_INDEX_PING: {
			atem_packet_broadcast_ping();
			break;
		}
	}
}
