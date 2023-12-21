#include <stdlib.h> // abort

#include "../utils/utils.h"



int main(void) {
	// Ensures successful closing request closes the session
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		// Ensures client is connected by sending a ping
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, sessionId, remoteId);
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_acknowledge_response_get_verify(packet, sessionId, remoteId);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, false, sessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, sessionId);
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a retransmitted closing request is handled normally
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);

		// Ensures client is connected by sending a ping
		uint16_t remoteId = 0x0001;
		atem_acknowledge_request_send(sock, sessionId, remoteId);
		uint8_t packet[ATEM_MAX_PACKET_LEN];
		while (atem_acknowledge_keepalive(sock, packet));
		atem_acknowledge_response_get_verify(packet, sessionId, remoteId);

		atem_handshake_sessionid_send(sock, ATEM_OPCODE_CLOSING, true, sessionId);
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_CLOSED, false, sessionId);
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures client reconnects after being closed
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t sessionId = atem_handshake_listen(sock, 0x0001);
		atem_handshake_close(sock, sessionId);
		atem_socket_norecv(sock);
		atem_socket_close(sock);

		sock = atem_socket_create();
		uint16_t reconnectSessionId = atem_handshake_start_server(sock);
		if (sessionId == reconnectSessionId) {
			fprintf(stderr,
				"Expected reconnect with non server session id, but got previous id: %02x %02x\n",
				sessionId, reconnectSessionId
			);
			abort();
		}
		atem_socket_close(sock);
	}

	return runner_exit();
}
