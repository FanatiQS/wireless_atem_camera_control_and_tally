#include <errno.h> // ECONNREFUSED
#include <stdlib.h> // abort
#include <stdio.h> // fprintf, stderr
#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t

#include "../utils/utils.h"

// Ensures the peer socket is closed and does not send any data
static void atem_client_closed(int sock) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	size_t packetLen = sizeof(packet);

	if (!simple_socket_recv_error(sock, ECONNREFUSED, packet, &packetLen)) {
		fprintf(stderr, "Expected no more data on socket: %zu\n", packetLen);
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

		uint8_t packet[ATEM_PACKET_LEN_MAX];
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

		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, sessionId);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client does not process any data after sending CLOSING request
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		uint8_t packet[ATEM_PACKET_LEN_MAX];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_acknowledge_request_send(sock, sessionId, 0x0001);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}



	// Ensures client responds to server initiated close correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, sessionId);

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

		atem_client_closed(sock);
	}

	// Ensures data sent after closing handshake is not processed
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		atem_handshake_close(sock, sessionId);
		atem_acknowledge_request_send(sock, sessionId, 0x0001);

		atem_client_closed(sock);
	}



	// Ensures client does not resend closing request a single time
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0012);

		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_recv(sock, packet);
		} while (!(atem_header_flags_get(packet) & ATEM_FLAG_SYN));
		atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures time without packet being acknowledged is never higher than ATEM_TIMEOUT_MS before being dropped
	RUN_TEST() {
		for (int i = 0; i < 10; i++) {
			int sock = atem_socket_create();
			uint16_t session_id = atem_handshake_listen(sock, 0x0001);

			uint8_t packet[ATEM_PACKET_LEN_MAX];
			struct timespec timeout_start = timediff_mark();
			atem_socket_recv(sock, packet);
			do {
				atem_socket_recv(sock, packet);
			} while (!(atem_header_flags_get(packet) & ATEM_FLAG_SYN));
			atem_handshake_sessionid_get_verify(packet, ATEM_OPCODE_CLOSING, false, session_id);

			int timeout_diff = timediff_get(timeout_start);
			if (timeout_diff > ATEM_TIMEOUT_MS) {
				fprintf(stderr, "Client closed session later than expected: %ld\n", timeout_diff);
				abort();
			}

			atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSED, false, session_id);
			atem_socket_close(sock);
		}
	}

	return runner_exit();
}
