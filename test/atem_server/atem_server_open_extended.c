#include "../utils/utils.h"

void atem_server_open_extended(void) {
	// Ensures client resends opening handshake response exactly ATEM_RESENDS times on a specified interval
	RUN_TEST() {
		uint16_t session_id_client = atem_header_sessionid_rand(false);
		int sock = atem_socket_create();
		uint16_t session_id_new = atem_handshake_start_client(sock, session_id_client);

		for (int i = 0; i < ATEM_RESENDS; i++) {
			struct timespec marker = timediff_mark();
			atem_handshake_newsessionid_recv_verify(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, session_id_new);
			timediff_get_verify(marker, 180, 30);
		}

		uint16_t session_id_server = session_id_new | 0x8000;
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSING, false, session_id_server);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, session_id_server);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}
}
