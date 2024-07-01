#include <assert.h> // assert

#include "../utils/utils.h"

// Ensures receiving packet is a retransmit request for the specified local_id
static void retransmit_request_recv_verify(int sock, uint16_t session_id, uint16_t local_id) {
	uint8_t packet[ATEM_PACKET_LEN_MAX];
	atem_socket_recv(sock, packet);
	atem_header_flags_get_verify(packet, ATEM_FLAG_RETXREQ, ATEM_FLAG_ACK);
	atem_header_len_get_verify(packet, ATEM_LEN_ACK);
	atem_header_sessionid_get_verify(packet, session_id);
	atem_header_remoteid_get_verify(packet, 0x0000);
	atem_header_localid_get_verify(packet, local_id);
}

int main(void) {
	// Ensures client acknowledges ping correctly
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x1337);

		atem_acknowledge_request_send(sock, session_id, 0x0001);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0001);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures a retransmitted packet is acknowledged if initial packet was dropped
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x5678);

		uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
		atem_acknowledge_request_set(packet, session_id, 0x0001);
		atem_header_flags_set(packet, ATEM_FLAG_RETX);
		atem_socket_send(sock, packet);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0001);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures a retransmitted packet is acknowledged even if initial packet was received and acknowledged too
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x5678);
		uint16_t remote_id = 0x0001;

		atem_acknowledge_request_send(sock, session_id, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id, remote_id);

		uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
		atem_acknowledge_request_set(packet, session_id, remote_id);
		atem_header_flags_set(packet, ATEM_FLAG_RETX);
		atem_socket_send(sock, packet);
		atem_acknowledge_response_recv_verify(sock, session_id, remote_id);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures there is no limit to number of retransmits allowed
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x5678);
		uint16_t remote_id = 0x0001;

		atem_acknowledge_request_send(sock, session_id, remote_id);
		atem_acknowledge_response_recv_verify(sock, session_id, remote_id);

		for (int i = 0; i < 100; i++) {
			uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
			atem_acknowledge_request_set(packet, session_id, remote_id);
			atem_header_flags_set(packet, ATEM_FLAG_RETX);
			atem_socket_send(sock, packet);
			atem_acknowledge_response_recv_verify(sock, session_id, remote_id);
		}

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}



	// Ensures last packet is acknowledged when old packet is received
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x1234);

		for (uint16_t remote_id = 1; remote_id < 0x0010; remote_id++) {
			atem_acknowledge_request_send(sock, session_id, remote_id);
			atem_acknowledge_response_recv_verify(sock, session_id, remote_id);
		}
		atem_acknowledge_request_send(sock, session_id, 0x0010);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0010);
		atem_acknowledge_request_send(sock, session_id, 0x0001);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0010);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures client requests packets it has not received when remote id is ahead in the sequence
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x1234);

		uint16_t local_id = 0x0005;
		for (uint16_t i = 1; i < local_id; i++) {
			atem_acknowledge_request_send(sock, session_id, local_id);
			retransmit_request_recv_verify(sock, session_id, i);
			atem_acknowledge_request_send(sock, session_id, i);

			// Acknowledgement is individually for each packet until packet 0x0004 could acknowledge 0x0005
			uint16_t ack_id = atem_acknowledge_response_recv(sock, session_id);
			if (ack_id != i) {
				assert(ack_id == local_id && i == local_id - 1);
			}
		}
	
		atem_acknowledge_request_send(sock, session_id, local_id);
		atem_acknowledge_response_recv_verify(sock, session_id, local_id);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures remote id closer to being behind than ahead in sequence is responded to with last acknowledged ack id
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0102);

		atem_acknowledge_request_send(sock, session_id, 0x4000);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0000);

		atem_acknowledge_request_send(sock, session_id, 0x0001);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0001);

		atem_acknowledge_request_send(sock, session_id, 0x4001);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0001);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures remote id closer to being ahead than behind in sequence is responded to with retransmit request
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_listen(sock, 0x0102);

		atem_acknowledge_request_send(sock, session_id, 0x3fff);
		retransmit_request_recv_verify(sock, session_id, 0x0001);

		atem_acknowledge_request_send(sock, session_id, 0x0001);
		atem_acknowledge_response_recv_verify(sock, session_id, 0x0001);

		atem_acknowledge_request_send(sock, session_id, 0x4000);
		retransmit_request_recv_verify(sock, session_id, 0x0002);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	return runner_exit();
}
