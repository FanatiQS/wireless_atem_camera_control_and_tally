#include "../utils/utils.h"

void atem_client_open(void) {
	// Ensures an unanswered opening handshake request is retransmitted at least ATEM_RESENDS times
	RUN_TEST() {
		// Gets initial opening handshake request from client
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_server(sock);

		// Ensures opening handshake is retransmitted ATEM_RESENDS times
		for (int i = 0; i < ATEM_RESENDS; i++) {
			atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, session_id);
		}

		atem_socket_close(sock);
	}

	// Ensures opening handshake is retried even after opening handshake request is sent ATEM_RESENDS times
	RUN_TEST() {
		atem_handshake_resetpeer();

		for (int i = 0; i < ATEM_RESENDS * 4; i++) {
			uint8_t packet[ATEM_PACKET_LEN_MAX];
			int sock = atem_socket_create();
			atem_socket_listen(sock, packet);
			atem_handshake_opcode_get_verify(packet, ATEM_OPCODE_OPEN);
			atem_socket_close(sock);
		}
	}



	// Ensures a successful opening handshake response connects client
	RUN_TEST() {
		// Gets opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id_client = atem_handshake_start_server(sock);

		// Completes opening handshake
		uint16_t session_id_new = atem_header_sessionid_rand(false);
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, session_id_client, session_id_new);
		atem_acknowledge_response_recv_verify(sock, session_id_client, 0x0000);

		// Ensures connected by sending ping
		uint16_t session_id_server = session_id_new | 0x8000;
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id_server, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id_server, remote_id);

		atem_handshake_close(sock, session_id_server);
		atem_socket_close(sock);
	}

	// Ensures a resent opening handshake also successfully opens
	RUN_TEST() {
		// Gets initial opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id_client = atem_handshake_start_server(sock);

		// Gets retransmitted opening handshake request
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, session_id_client);

		// Completes opening handshake
		uint16_t session_id_new = atem_header_sessionid_rand(false);
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, session_id_client, session_id_new);
		atem_acknowledge_response_recv_verify(sock, session_id_client, 0x0000);

		// Ensures connected by sending ping
		uint16_t session_id_server = session_id_new | 0x8000;
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id_server, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id_server, remote_id);

		atem_handshake_close(sock, session_id_server);
		atem_socket_close(sock);
	}

	// Ensures a resent accept is responded to without retransmit flag
	RUN_TEST() {
		// Gets initial opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id_client = atem_handshake_start_server(sock);

		// Completes opening handshake
		uint16_t session_id_new = atem_header_sessionid_rand(false);
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, false, session_id_client, session_id_new);
		atem_acknowledge_response_recv_verify(sock, session_id_client, 0x0000);

		// Retransmits response to opening handshake
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, session_id_new);
		atem_acknowledge_response_recv_verify(sock, session_id_client, 0x0000);

		// Ensures connected by sending ping
		uint16_t session_id_server = session_id_new | 0x8000;
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id_server, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id_server, remote_id);

		atem_handshake_close(sock, session_id_server);
		atem_socket_close(sock);
	}

	// Ensures resent opening handshake response is acknowledged if initial response was dropped
	RUN_TEST() {
		// Gets initial opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id_client = atem_handshake_start_server(sock);

		// Completes opening handshake with retransmit flag
		uint16_t session_id_new = atem_header_sessionid_rand(false);
		atem_handshake_newsessionid_send(sock, ATEM_OPCODE_ACCEPT, true, session_id_client, session_id_new);
		atem_acknowledge_response_recv_verify(sock, session_id_client, 0x0000);

		// Ensures connected by sending ping
		uint16_t session_id_server = session_id_new | 0x8000;
		uint16_t remote_id = 0x0001;
		atem_acknowledge_request_send(sock, session_id_server, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id_server, remote_id);

		atem_handshake_close(sock, session_id_server);
		atem_socket_close(sock);
	}



	// Ensures no messages are sent after responding to client with a reject response
	RUN_TEST() {
		// Gets opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_server(sock);

		// Rejects opening handshake request
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, session_id);

		// Ensures no more data is transmitted
		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a reject response to a resent open request is handled as a normal reject
	RUN_TEST() {
		// Gets initial opening handshake request
		atem_handshake_resetpeer();
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_start_server(sock);

		// Gets retransmitted opening handshake request and rejects
		atem_handshake_sessionid_recv_verify(sock, ATEM_OPCODE_OPEN, true, session_id);
		atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, session_id);

		atem_socket_norecv(sock);
		atem_socket_close(sock);
	}

	// Ensures a rejected ATEM client retries connecting and does not give up retrying
	RUN_TEST() {
		atem_handshake_resetpeer();
		for (int i = 0; i < ATEM_RESENDS + 2; i++) {
			int sock = atem_socket_create();
			uint16_t session_id = atem_handshake_start_server(sock);

			atem_handshake_sessionid_send(sock, ATEM_OPCODE_REJECT, false, session_id);

			atem_socket_close(sock);
		}
	}



	// Ensures the client reconnects within time frame when connection is dropped
	RUN_TEST() {
		// Client connects
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, atem_header_sessionid_rand(false));

		// Drops all packets for ATEM_TIMEOUT seconds
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		struct timespec timeout_start = timediff_mark();
		while (simple_socket_poll(sock, ATEM_TIMEOUT_MS - timediff_get(timeout_start))) {
			atem_socket_recv(sock, packet);
			atem_header_sessionid_get_verify(packet, session_id);
		}

		// Ensures client reconnects after being dropped
		atem_socket_close(sock);
		sock = atem_socket_create();
		session_id = atem_handshake_listen(sock, atem_header_sessionid_rand(false));

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures the client reconnects without retransmit flag set
	RUN_TEST() {
		// Client connects
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, atem_header_sessionid_rand(false));

		// Ensures retransmit flag is cleared when new session id is used
		uint8_t packet[ATEM_PACKET_LEN_MAX];
		do {
			atem_socket_close(sock);
			sock = atem_socket_create();
			atem_socket_listen(sock, packet);
		} while (atem_header_sessionid_get(packet) == session_id);
		atem_handshake_sessionid_get(packet, ATEM_OPCODE_OPEN, false);

		atem_socket_close(sock);
	}
}
