#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <assert.h> // assert
#include <stdint.h> // uint32_t
#include <limits.h> // INT_MAX

#include "./atem_server.h" // atem_server
#include "./atem_packet.h" // atem_packet_broadcast_ping
#include "./atem_debug.h" // DEBUG_PRINTF
#include "./timeout.h"

// Gets remaining time between current time and specified timeout
static unsigned int timeout_remaining(struct timespec* now, struct timespec* timeout, uint16_t delay) {
	assert(now != NULL);
	assert(timeout != NULL);

	// Gets number of milliseconds since timeout was defined
	time_t delta = (now->tv_sec - timeout->tv_sec) * 1000 + (now->tv_nsec - timeout->tv_nsec) / 1000000;

	// Returns number of milliseconds remaining of delay or 0 if it has already passed
	if (delta > delay) {
		return 0;
	}
	return (unsigned int)(delay - delta);
}

// Dispatches timeouts and gets milliseconds to next timeout or -1 for no timeout available
int timeout_get(void) {
	// Timeout to return, unsigned so -1 is seen as largest number instead of smaller than everything
	unsigned int timeout = (unsigned int)-1;

	// Gets current time to calculate remaining time for timeouts
	struct timespec now;
	int timespec_result = timespec_get(&now, TIME_UTC);
	assert(timespec_result == TIME_UTC);

	// Gets ATEM server ping timeout
	if (atem_server.sessions_connected > 0) {
		// Gets remaining time for next ATEM server ping
		unsigned int timeout_ping = timeout_remaining(&now, &atem_server.ping_timestamp, atem_server.ping_interval);

		// Broadcasts ping if timeout has expired
		if (timeout_ping == 0) {
			atem_packet_broadcast_ping(&now);
			assert(atem_server.ping_timestamp.tv_sec >= now.tv_sec);
			assert(atem_server.ping_timestamp.tv_nsec >= now.tv_nsec);
			assert(atem_server.packet_queue_head != NULL);
			DEBUG_PRINTF("Timeout ping: %dms\n", timeout_ping);
		}
		// Exposes ping timeout since it could potentially be the closes timeout
		else {
			DEBUG_PRINTF("Timeout ping: %dms\n", timeout_ping);
			timeout = timeout_ping;
		}
	}
	assert(timeout > 0);

	// Resends all retransmits that has expired and gets retransmit timeout if closer in time than ping timeout
	while (atem_server.packet_queue_head != NULL) {
		unsigned int timeout_retx;
		timeout_retx = timeout_remaining(&now, &atem_server.packet_queue_head->timestamp, atem_server.retransmit_delay);
		if (timeout_retx > 0) {
			DEBUG_PRINTF("Timeout retransmit: %dms\n", timeout_retx);
			if (timeout_retx < timeout) {
				timeout = timeout_retx;
			}
			break;
		}
		atem_packet_retransmit(&now);
	}
	assert(timeout > 0);

	if (timeout == ~0u) {
		DEBUG_PRINTF("No timeout\n\n");
	}
	else {
		DEBUG_PRINTF("Timeout: %dms\n\n", timeout);
	}

	assert(timeout == (unsigned int)-1 || timeout < INT_MAX);
	return (int)timeout;
}
