#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // false
#include <time.h> // struct timespec
#include <assert.h> // assert

#include "../utils/utils.h"

int main(void) {
	// Ensures packet with length over soft max size is not acknowledged
	RUN_TEST() {
		// Connects to ATEM switcher
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, atem_header_sessionid_rand(false));

		// Sends packet with max size
		uint16_t remote_id = 0x0001;
		uint8_t payload[ATEM_PACKET_LEN_MAX_SOFT - ATEM_LEN_HEADER - ATEM_LEN_CMDHEADER + 1] = {0};
		assert(atem_command_send(sock, session_id, remote_id, "test", payload, sizeof(payload)) == (ATEM_PACKET_LEN_MAX_SOFT + 1));

		// Will not receive acknowledgement for over max size request
		struct timespec mark = timediff_mark();
		while (timediff_get(mark) < 2000) {
			uint8_t packet[ATEM_PACKET_LEN_MAX];
			atem_acknowledge_keepalive(sock, packet);
			if (atem_header_flags_get(packet) & ATEM_FLAG_ACK) {
				atem_header_ackid_get_verify(packet, 0x0000);
			}
		}

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	return runner_exit();
}
