// This should ensure that all commands received from the switcher being tested exists in the list
// It should not ensure that all commands are received since not all switcher modes support all the features

#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stdbool.h> // bool, true, false
#include <stddef.h> // size_t

#include "../utils/utils.h"

// Known ATEM commands
static uint32_t cmds[] = {
	ATEM_CMDNAME_VERSION, // _ver
	ATEM_CMDNAME('_', 'p', 'i', 'n'), // _pin
	ATEM_CMDNAME('_', 't', 'o', 'p'), // _top
	ATEM_CMDNAME('_', 'M', 'e', 'C'), // _MeC
	ATEM_CMDNAME('_', 'm', 'p', 'l'), // _mpl
	ATEM_CMDNAME('_', 'M', 'v', 'C'), // _MvC
	ATEM_CMDNAME('_', 'A', 'M', 'C'), // _AMC
	ATEM_CMDNAME('_', 'V', 'M', 'C'), // _VMC
	ATEM_CMDNAME('_', 'M', 'A', 'C'), // _MAC
	ATEM_CMDNAME('P', 'o', 'w', 'r'), // Powr
	ATEM_CMDNAME('V', 'i', 'd', 'M'), // VidM
	ATEM_CMDNAME('T', 'c', 'L', 'k'), // TcLk
	ATEM_CMDNAME('I', 'n', 'P', 'r'), // InPr
	ATEM_CMDNAME('M', 'v', 'V', 'M'), // MvVM
	ATEM_CMDNAME('M', 'v', 'P', 'r'), // MvPr
	ATEM_CMDNAME('M', 'v', 'I', 'n'), // MvIn
	ATEM_CMDNAME('P', 'r', 'g', 'I'), // PrgI
	ATEM_CMDNAME('P', 'r', 'v', 'I'), // PrvI
	ATEM_CMDNAME('T', 'r', 'S', 'S'), // TrSS
	ATEM_CMDNAME('T', 'r', 'P', 'r'), // TrPr
	ATEM_CMDNAME('T', 'r', 'P', 's'), // TrPs
	ATEM_CMDNAME('T', 'M', 'x', 'P'), // TMxP
	ATEM_CMDNAME('T', 'D', 'p', 'P'), // TDpP
	ATEM_CMDNAME('T', 'W', 'p', 'P'), // TWpP
	ATEM_CMDNAME('T', 'D', 'v', 'P'), // TDvP
	ATEM_CMDNAME('T', 'S', 't', 'P'), // TStP
	ATEM_CMDNAME('K', 'e', 'O', 'n'), // KeOn
	ATEM_CMDNAME('K', 'e', 'B', 'P'), // KeBP
	ATEM_CMDNAME('K', 'e', 'L', 'm'), // KeLm
	ATEM_CMDNAME('K', 'e', 'C', 'k'), // KeCk
	ATEM_CMDNAME('K', 'e', 'P', 't'), // KePt
	ATEM_CMDNAME('K', 'e', 'D', 'V'), // KeDV
	ATEM_CMDNAME('K', 'e', 'F', 'S'), // KeFS
	ATEM_CMDNAME('K', 'K', 'F', 'P'), // KKFP
	ATEM_CMDNAME('D', 's', 'k', 'B'), // DskB
	ATEM_CMDNAME('D', 's', 'k', 'P'), // DskP
	ATEM_CMDNAME('D', 's', 'k', 'S'), // DskS
	ATEM_CMDNAME('F', 't', 'b', 'P'), // FtbP
	ATEM_CMDNAME('F', 't', 'b', 'S'), // FtbS
	ATEM_CMDNAME('C', 'o', 'l', 'V'), // ColV
	ATEM_CMDNAME('M', 'P', 'f', 'e'), // MPfe
	ATEM_CMDNAME('M', 'P', 'C', 'E'), // MPCE
	ATEM_CMDNAME('R', 'X', 'M', 'S'), // RXMS
	ATEM_CMDNAME('R', 'X', 'C', 'P'), // RXCP
	ATEM_CMDNAME('R', 'X', 'S', 'S'), // RXSS
	ATEM_CMDNAME('R', 'X', 'C', 'C'), // RXCC
	ATEM_CMDNAME('A', 'M', 'P', 'P'), // AMPP
	ATEM_CMDNAME('A', 'M', 'I', 'P'), // AMIP
	ATEM_CMDNAME('A', 'M', 'M', 'O'), // AMMO
	ATEM_CMDNAME('A', 'M', 'T', 'l'), // AMTl
	ATEM_CMDNAME('L', 'K', 'S', 'T'), // LKST
	ATEM_CMDNAME('_', 'T', 'l', 'C'), // _TlC
	ATEM_CMDNAME_TALLY, // TlIn
	ATEM_CMDNAME('T', 'l', 'S', 'r'), // TlSr
	ATEM_CMDNAME('T', 'l', 'F', 'c'), // TlFc
	ATEM_CMDNAME('M', 'R', 'P', 'r'), // MRPr
	ATEM_CMDNAME('M', 'R', 'c', 'S'), // MRcS
	ATEM_CMDNAME('M', 'P', 'r', 'p'), // MPrp
	ATEM_CMDNAME('C', 'C', 's', 't'), // CCst
	ATEM_CMDNAME('I', 'n', 'C', 'm'), // InCm
	ATEM_CMDNAME_CAMERACONTROL, // CCdP
};

int main(void) {
	// Ensures packet with max size is acknowledged
	RUN_TEST() {
		// Connects to ATEM switcher
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, atem_header_sessionid_rand(false));

		// Sends packet with max size
		uint16_t remote_id = 0x0001;
		uint8_t payload[ATEM_PACKET_LEN_MAX_SOFT - ATEM_LEN_HEADER - ATEM_LEN_CMDHEADER] = {0};
		atem_command_send(sock, session_id, remote_id, "test", payload, sizeof(payload));

		// Receives acknowledgement for max size request
		atem_acknowledge_response_flush(sock, session_id, remote_id);

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures packet with over max size is not acknowledged
	RUN_TEST() {
		// Connects to ATEM switcher
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, atem_header_sessionid_rand(false));

		// Sends packet with max size
		uint16_t remote_id = 0x0001;
		uint8_t payload[ATEM_PACKET_LEN_MAX_SOFT - ATEM_LEN_HEADER - ATEM_LEN_CMDHEADER + 1] = {0};
		atem_command_send(sock, session_id, remote_id, "test", payload, sizeof(payload));

		// Will not receive acknowledgement for over max size request
		struct timespec mark = timediff_mark();
		while (timediff_get(mark) < 2000) {
			uint8_t packet[ATEM_PACKET_LEN_MAX];
			atem_acknowledge_keepalive(sock, packet);
			if (atem_header_flags_get(packet) & ATEM_FLAG_ACK) {
				atem_header_ackid_get_verify(packet, 0x0000);
			}
		}

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
	}

	// Ensures no unknown commands are being transmitted by switcher at full state dump
	RUN_TEST() {
		// Connects to ATEM switcher
		int sock = atem_socket_create();
		uint16_t session_id = atem_handshake_connect(sock, atem_header_sessionid_rand(false));

		bool failed = false;
		int pings = 2;
		while (pings) {
			// Reads ATEM packet up to receiving more than 1 ping to indicate full state should have been transmitted
			uint8_t packet[ATEM_PACKET_LEN_MAX] = {0};
			atem_acknowledge_keepalive(sock, packet);
			if (atem_header_len_get(packet) == ATEM_LEN_HEADER) {
				pings--;
				continue;
			}

			// Ensures that all received commands are known
			for (
				uint16_t offset = ATEM_LEN_HEADER;
				offset < atem_header_len_get(packet);
				offset += atem_packet_word_get(packet + offset, 0, 1)
			) {
				bool found = false;
				uint8_t* buf = packet + offset + 4;
				for (size_t i = 0; i < (sizeof(cmds) / sizeof(cmds[0])); i++) {
					if ((uint32_t)ATEM_CMDNAME(buf[0], buf[1], buf[2], buf[3]) == cmds[i]) {
						found = true;
						break;
					}
				}
				if (found == false) {
					failed = true;
					fprintf(stderr, "Command \"%.4s\" unknown\n", buf);
				}
			}
		}

		atem_handshake_close(sock, session_id);
		atem_socket_close(sock);
		if (failed) {
			abort();
		}
	}

	return runner_exit();
}
