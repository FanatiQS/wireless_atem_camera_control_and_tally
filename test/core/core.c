#include <stdint.h> // UINT16_MAX, uint16_t
#include <stdbool.h> // true, false
#include <assert.h> // assert
#include <string.h> // strlen, strcmp
#include <stddef.h> // size_t

#include "../utils/utils.h"

int main(void) {
	// Ensures ATEM_PACKET_LEN_MAX equals to the max length left after bits
	RUN_TEST() {
		assert(ATEM_PACKET_LEN_MAX == (UINT16_MAX >> 5));
	}

	// Ensures accepted handshake returns ATEM_STATUS_ACCEPTED once
	RUN_TEST() {
		struct atem atem = {0};
		atem_connection_reset(&atem);
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_ACCEPT, true, atem_header_sessionid_next(false));
		assert(atem_parse(&atem) == ATEM_STATUS_ACCEPTED);
		assert(atem_parse(&atem) == ATEM_STATUS_WRITE_ONLY);
	}

	// Ensures rejected handshake returns ATEM_STATUS_REJECTED
	RUN_TEST() {
		struct atem atem = {0};
		atem_connection_reset(&atem);
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_REJECT, true, atem_header_sessionid_next(false));
		assert(atem_parse(&atem) == ATEM_STATUS_REJECTED);
		assert(atem_parse(&atem) == ATEM_STATUS_REJECTED);
	}

	// Ensures received command returns ATEM_STATUS_WRITE once per packet
	RUN_TEST() {
		struct atem atem = {0};
		atem_acknowledge_request_set(atem.read_buf, atem_header_sessionid_next(true), 0x0001);
		assert(atem_parse(&atem) == ATEM_STATUS_WRITE);
		assert(atem_parse(&atem) == ATEM_STATUS_WRITE_ONLY);
	}

	// Ensures unexpected flags returns ATEM_STATUS_NONE
	RUN_TEST() {
		struct atem atem = {0};
		atem_acknowledge_response_set(atem.read_buf, atem_header_sessionid_next(true), 0x0001);
		assert(atem_parse(&atem) == ATEM_STATUS_NONE);
		assert(atem_parse(&atem) == ATEM_STATUS_NONE);
	}

	// Ensures closing handshake request returns ATEM_STATUS_CLOSING once
	RUN_TEST() {
		struct atem atem = {0};
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_CLOSING, false, atem_header_sessionid_next(true));
		assert(atem_parse(&atem) == ATEM_STATUS_CLOSING);
		assert(atem_parse(&atem) == ATEM_STATUS_WRITE_ONLY);
	}

	// Ensures closed session response returns ATEM_STATUS_CLOSED
	RUN_TEST() {
		struct atem atem = {0};
		atem_connection_close(&atem);
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_CLOSED, false, atem_header_sessionid_next(true));
		assert(atem_parse(&atem) == ATEM_STATUS_CLOSED);
		assert(atem_parse(&atem) == ATEM_STATUS_CLOSED);
	}

	// Ensures invalid client opcode returns ATEM_STATUS_ERROR
	RUN_TEST() {
		struct atem atem = {0};
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_OPEN, false, atem_header_sessionid_next(false));
		assert(atem_parse(&atem) == ATEM_STATUS_ERROR);
		assert(atem_parse(&atem) == ATEM_STATUS_ERROR);
	}

	// Ensures commands are extracted correctly
	RUN_TEST() {
		// Creates ATEM context with read buffer filled with commands
		struct atem atem = {0};
		atem_acknowledge_request_set(atem.read_buf, atem_header_sessionid_next(false), 0x0001);
		char* payload_buf = "this is a test payload";
		const uint16_t payload_len = strlen(payload_buf) & ATEM_PACKET_LEN_MAX;
		for (int i = 0; i < (ATEM_PACKET_LEN_MAX / (payload_len + ATEM_LEN_CMDHEADER) - 1); i++) {
			atem_command_append(atem.read_buf, "TEST", payload_buf, payload_len);
		}

		// Ensures commands are extracted correctly
		assert(atem_parse(&atem) == ATEM_STATUS_WRITE);
		size_t len_remaining = atem.read_len;
		while (atem_cmd_available(&atem)) {
			atem_cmd_next(&atem);
			assert(atem.cmd_buf > atem.read_buf);
			assert((atem.cmd_buf - atem.read_buf) < atem.read_len);
			assert(atem.cmd_len < atem.read_len);
			assert(strcmp((char*)atem.cmd_buf, payload_buf) == 0);
			assert(atem.cmd_len == (payload_len + ATEM_LEN_CMDHEADER));
			len_remaining -= atem.cmd_len;
		}
		assert(len_remaining == ATEM_LEN_HEADER);
		assert(atem.cmd_index == atem.read_len);
	}

	// Ensures stored remote id is incremented and reset correctly
	RUN_TEST() {
		struct atem atem = {0};
		assert(atem.remote_id_last == 0);

		// Acknowledges multiple acknowledge requests
		for (uint16_t i = 1; i < 10; i++) {
			atem_acknowledge_request_set(atem.read_buf, 0x0001, i);
			assert(atem_parse(&atem) == ATEM_STATUS_WRITE);
			assert(atem.remote_id_last == i);
		}

		// Ensures remote id is reset when with new connection
		atem_connection_reset(&atem);
		atem_packet_clear(atem.read_buf);
		atem_handshake_sessionid_set(atem.read_buf, ATEM_OPCODE_ACCEPT, false, 0x7832);
		assert(atem_parse(&atem) == ATEM_STATUS_ACCEPTED);
		assert(atem.remote_id_last == 0);
	}

	return runner_exit();
}
