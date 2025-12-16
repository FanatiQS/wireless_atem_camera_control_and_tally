#include <stdlib.h> // abort
#include <stdint.h> // uint8_t, uint16_t

#include "../utils/utils.h"

void atem_client_close(void) {
	// Ensures successful closing request closes the session
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0001);

		// Ensures client is connected by sending a ping
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id, remote_id);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_acknowledge_response_get_verify(packet, session_id, remote_id);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, session_id);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, session_id);
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a retransmitted closing request is handled normally
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0001);

		// Ensures client is connected by sending a ping
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id, remote_id);
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_acknowledge_response_get_verify(packet, session_id, remote_id);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, true, session_id);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, session_id);
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client reconnects after being closed
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0001);
		atem_handshake_close(sock, session_id);
		atem_socket_norecv(sock);
		atem_socket_close(sock);

		sock = atem_socket_create();
		uint16_t session_id_reconnect = atem_handshake_start_server(sock);
		if (session_id == session_id_reconnect) {
			fprintf(stderr,
				"Expected reconnect with non server session id, but got previous id: %02x %02x\n",
				session_id, session_id_reconnect
			);
			abort();
		}
		atem_socket_close(sock);
	}



	// Ensures not acknowledging packets causes client to close connection
	RUN_TEST() {
		// Client connects
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0001);

		// Waits from client to close
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (atem_header_flags_get(packet) & ATEM_FLAG_ACKREQ);
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, session_id);
		atem_socket_norecv(sock);
	}
}
