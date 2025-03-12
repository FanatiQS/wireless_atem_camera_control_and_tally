#include <stdio.h> // fprintf, stderr, printf
#include <stdint.h> // uint8_t, uint16_t, int16_t
#include <time.h> // time_t, struct timespec, timespec_get
#include <stddef.h> // size_t, NULL

#include "./atem_packet.h" // struct atem_packet, struct atem_packet_session
#include "./atem_server.h" // atem_server
#include "./atem_session.h" // atem_session_lookup_get, atem_session_get
#include "../core/atem_protocol.h" // ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW
#include "../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "./atem_debug.h"

#if DEBUG

// Prints buffer to stderr
void atem_debug_print_buf(const uint8_t* buf, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		if (i != 0 && !(i % 32)) {
			fprintf(stderr, "\n\t");
		}
		fprintf(stderr, "%02x ", buf[i]);
	}
	fprintf(stderr, "\n");
}



// Start time of process used for printing time diff
static time_t atem_debug_timeout_start;

// Initializes debug information
__attribute__((constructor)) static void atem_debug_init(void) {
	// Initializes process start time
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	atem_debug_timeout_start = ts.tv_sec;
}

// Prints formatted packet data
void atem_debug_print_packet(struct atem_packet* packet) {
	// Prints packet data
	printf("%p:\n", (void*)packet);
	printf("\t" "resends remaining: %d\n", packet->resends_remaining);
	printf("\t" "closing: %s\n", (packet->flags & ATEM_PACKET_FLAG_CLOSING) ? "YES" : "NO");
	printf("\t" "release buf: %s\n", (packet->flags & ATEM_PACKET_FLAG_RELEASE) ? "YES" : "NO");
	printf(
		"\t" "timeout timestamp: %ld.%ld\n",
		packet->timeout.tv_sec - atem_debug_timeout_start,
		packet->timeout.tv_nsec / 1000000
	);
	printf("\t" "sessions remaining: %d\n", packet->sessions_remaining);
	printf("\t" "sessions len: %d\n", packet->sessions_len);

	// Prints packet buffer and length
	uint16_t len = (packet->buf[ATEM_INDEX_LEN_HIGH] << 8 | packet->buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;
	printf("\t" "buffer length: %u\n", len);
	printf("\t");
	int indent = printf("buffer: ");
	for (uint16_t i = 0; i < len; i++) {
		if (i != 0 && !(i % 32)) {
			printf("\n\t%*c", indent, ' ');
		}
		printf("%02x ", packet->buf[i]);
	}
	printf("\n");

	// Prints connected sessions data
	printf("\t" "sessions:\n");
	for (uint16_t i = 0; i < packet->sessions_len; i++) {
		if (i == packet->sessions_remaining) {
			printf("\t" "================================\n");
		}
		else if (i != 0) {
			printf("\t" "--------------------------------\n");
		}
		struct atem_packet_session* packet_session = &packet->sessions[i];
		printf("\t\t" "session_id: %04x\n", packet_session->session_id);
		printf("\t\t" "packet_session_index: %d\n", packet_session->packet_session_index);
		printf("\t\t" "packet_next: %p\n", (void*)packet_session->packet_next);
		printf("\t\t" "packet_session_index_next: %d\n", packet_session->packet_session_index_next);
		printf("\t\t" "remote_id: %d\n", packet_session->remote_id_high << 8 | packet_session->remote_id_low);
	}
}

// Prints all packets in packet queue
void atem_debug_print_packets(void) {
	printf("======== Packet Queue ========\n");
	for (
		struct atem_packet* packet = atem_server.packet_queue_head;
		packet != NULL;
		packet = packet->next
	) {
		atem_debug_print_packet(packet);
	}
}

// Prints formatted session data
void atem_debug_print_session(struct atem_session* session) {
	printf("%p:\n", (void*)session);
	printf("\t" "session_index: %d\n", atem_session_lookup_get(session->session_id));
	printf("\t" "remote_id: %d\n", session->remote_id);
	printf("\t" "session_id: 0x%04x\n", session->session_id);
	printf("\t" "session_id2: 0x%02x%02x\n", session->session_id_high, session->session_id_low);
	printf("\t" "packet_tail: %p\n", (void*)session->packet_tail);
	printf("\t" "packet_session_index_head: %d\n", session->packet_session_index_head);
	printf("\t" "packet_session_index_tail: %d\n", session->packet_session_index_tail);

	printf("\t" "packets:\n");
	struct atem_packet* packet = session->packet_head;
	uint16_t packet_session_index = session->packet_session_index_head;
	while (packet != NULL) {
		printf("\t\t" "%p\n", (void*)packet);
		struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
		packet_session_index = packet_session->packet_session_index_next;
		packet = packet_session->packet_next;
	}
}

// Prints all sessions
void atem_debug_print_sessions(void) {
	printf("Sessions connected: %d\n", atem_server.sessions_connected);
	printf("======== Sessions ========\n");
	for (int16_t i = 0; i < atem_server.sessions_len; i++) {
		if (i != 0 && i == atem_server.sessions_connected) {
			printf("--------------------------\n");
		}
		atem_debug_print_session(atem_session_get(i));
	}
}

// Prints session id to session index lookup table
void atem_debug_print_table(void) {
	printf("======== Lookup Table ========\n");
	const size_t size = sizeof(atem_server.session_lookup_table) / sizeof(atem_server.session_lookup_table[0]);
	for (size_t i = 0; i < size; i++) {
		int16_t session_index = atem_session_lookup_get(i);
		if (session_index != -1) {
			printf("0x%04zx: %d\n", i, session_index);
		}
	}
}

// Prints all ATEM server data
void atem_debug_print_server(void) {
	atem_debug_print_sessions();
	printf("\n");
	atem_debug_print_table();
	printf("\n");
	atem_debug_print_packets();
}

// Prints all global ATEM states
void atem_debug_print(void) {
	atem_debug_print_server();
}

#endif // DEBUG
