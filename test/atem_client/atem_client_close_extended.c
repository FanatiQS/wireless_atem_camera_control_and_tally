#include <errno.h> // ECONNREFUSED
#include <stdlib.h> // abort
#include <stdio.h> // fprintf, stderr
#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t

#include "../utils/utils.h"

// Ensures the peer socket is closed and does not send any data
void atem_client_close_extended_closed(int sock) {
	uint8_t packet[ATEM_MAX_PACKET_LEN];
	size_t packetLen = sizeof(packet);

	if (!simple_socket_recv_error(sock, ECONNREFUSED, packet, &packetLen)) {
		fprintf(stderr, "Expected no response to ping: %zu\n", packetLen);
		logs_print_buffer(stderr, packet, packetLen);
		abort();
	}

	atem_socket_close(sock);
}

int main(void) {
	// Ensures client closes session correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		uint8_t packet[ATEM_MAX_PACKET_LEN];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client does not retransmit closing request if not answered
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		uint8_t packet[ATEM_MAX_PACKET_LEN];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures retransmitted closing request is not responded to
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, sessionId);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, true, sessionId);

		atem_client_close_extended_closed(sock);
	}

	// Ensures data sent after closing handshake is not processed
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		atem_handshake_close(sock, sessionId);
		atem_acknowledge_request_send(sock, sessionId, 0x0001);

		atem_client_close_extended_closed(sock);
	}

	return runner_exit();
}
