#include <assert.h> // assert
#include <stdbool.h> // bool, true, false
#include <stdio.h> // fprintf, stdout
#include <stddef.h> // size_t
#include <stdlib.h> // abort

#include "../utils/utils.h"

// Camera control category and parameter combos from SDI Camera Control protocol to check
static uint16_t cc_params_to_check[] = {
	0x0000, 0x0002, // Lens
	0x0101, 0x0102, 0x0105, 0x0108, 0x010d, 0x0110, // Video
	0x0404, // Display
	0x0800, 0x0801, 0x0802, 0x0803, 0x0804, 0x0805, 0x0806, // Color Correction
	0x0b00 // PTZ Control
};
static const size_t cc_params_to_check_length = (sizeof(cc_params_to_check) / sizeof(cc_params_to_check[0]));

// Validates that a received category and parameter combo is expected and stores it it the checklist
static void camera_control_check(uint8_t category, uint8_t parameter, size_t* checklist) {
	uint16_t category_and_paramter = category << 8 | parameter;
	for (size_t i = 0; i < cc_params_to_check_length; i++) {
		if (cc_params_to_check[i] == category_and_paramter) {
			*checklist &= ~(1 << i);
			return;
		}
	}
	fprintf(
		stderr,
		"Received unexpected camera control category and/or parameter: %d,%d\n",
		category, parameter
	);
}

int main(void) {
	// Ensures all expected camera control parameters at connect are received and only those parameters
	RUN_TEST() {
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, 0x1234);

		struct atem atem = { .dest = 1 };
		atem_connection_reset(&atem);
		atem_handshake_opcode_set(atem.read_buf, ATEM_OPCODE_ACCEPT);
		assert(atem_parse(&atem) == ATEM_STATUS_ACCEPTED);

		// Ensures all camera control parameters received when connected are all expected
		size_t checklist = (1 << cc_params_to_check_length) - 1;
		do {
			// Reads all commands from state dump packets
			atem_socket_recv(sock, atem.read_buf);
			enum atem_status status = atem_parse(&atem);
			assert(status == ATEM_STATUS_WRITE);
			atem_header_len_get_verify(atem.write_buf, atem.write_len);
			atem_socket_send(sock, atem.write_buf);

			// Validates camera control parameters in packet
			while (atem_cmd_available(&atem)) {
				if (atem_cmd_next(&atem) != ATEM_CMDNAME_CAMERACONTROL) continue;
				if (!atem_cc_updated(&atem)) continue;
				atem_cc_translate(&atem);
				camera_control_check(atem.cmd_buf[4], atem.cmd_buf[5], &checklist);
			}
		} while (atem_header_len_get(atem.read_buf) != ATEM_LEN_HEADER);

		// Closes ATEM connection
		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);

		// Ensures all required camera control parameters where received
		bool failed = false;
		for (size_t i = 0; i < cc_params_to_check_length; i++) {
			if (!((checklist >> i) & 0x01)) continue;
			failed = true;
			fprintf(
				stderr,
				"Missing parameter %d,%d\n",
				cc_params_to_check[i] >> 8, cc_params_to_check[i] & 0xff
			);
		}
		if (failed == true) {
			abort();
		}
	}

	return runner_exit();
}
