#include "../utils/utils.h"

int main(void) {
	// Ensures client resends opening handshake response exactly ATEM_RESENDS times on a specified interval
	RUN_TEST() {
		uint16_t clientSessionId = 0x1337;
		int sock = atem_socket_create();
		uint16_t newSessionId = atem_handshake_start_client(sock, clientSessionId);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			struct timespec marker = timediff_mark();
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, clientSessionId, newSessionId);
			timediff_get_verify(marker, 180, 30);
		}

		uint16_t serverSessionId = newSessionId | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, serverSessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, serverSessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	return runner_exit();
}
