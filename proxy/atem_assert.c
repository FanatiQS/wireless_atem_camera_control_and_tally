#include <assert.h> // assert
#include <stdbool.h> // true, false
#include <stddef.h> // NULL
#include <stdint.h> // uint16_t, int16_t
#include <time.h> // struct timespec, timespec_get, TIME_UTC, time_t

#include "./atem_server.h" // atem_server
#include "./atem_session.h" // struct atem_session, atem_session_lookup_get, atem_session_get
#include "./atem_packet.h" // struct atem_packet, struct atem_packet_session
#include "../core/atem_protocol.h" // ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW, ATEM_MAX_LEN_HIGH, ATEM_INDEX_FLAGS, ATEM_LEN_HEADER, ATEM_LEN_SYN
#include "../core/atem.h" // ATEM_PACKET_LEN_MAX, ATEM_TIMEOUT
#include "./atem_assert.h"

// Does not assert server data structures in release build
#ifndef NDEBUG

// Asserts a specified packet
void atem_assert_packet(struct atem_packet* packet) {
	assert(packet->sessions_remaining > 0);

	uint16_t len = (packet->buf[ATEM_INDEX_LEN_HIGH] << 8 | packet->buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;
	assert(len >= ATEM_LEN_HEADER);
	assert(len <= ATEM_PACKET_LEN_MAX);
	assert(packet->buf[ATEM_INDEX_FLAGS] & ~(ATEM_PACKET_LEN_MAX >> 8));

	// Asserts no packets are orphaned
	for (uint16_t i = 0; i < packet->sessions_remaining; i++) {
		uint16_t packet_session_index = packet->sessions[i].packet_session_index;
		struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
		int16_t session_index = atem_session_lookup_get(packet_session->session_id);
		struct atem_session* session = atem_session_get(session_index);
		struct atem_packet* packet_find = session->packet_head;
		uint16_t packet_session_index_find = session->packet_session_index_head;
		while (packet != packet_find) {
			assert(packet_find != NULL);
			struct atem_packet_session* ps = atem_packet_session_get(packet_find, packet_session_index_find);
			packet_find = ps->packet_next;
			packet_session_index_find = ps->packet_session_index_next;
		}
	}

	// Asserts no session ids occur more than once
	for (uint16_t i = 0; i < packet->sessions_remaining; i++) {
		for (uint16_t j = 0; j < packet->sessions_remaining; j++) {
			if (i != j) {
				assert(packet->sessions[i].session_id != packet->sessions[j].session_id);
			}
		}
	}
}

// Asserts all packets through global packet queue
void atem_assert_packets(void) {
	struct atem_packet* packet = atem_server.packet_queue_head;
	if (packet == NULL) return;

	// Gets current time to calculate timeout remaining from
	struct timespec now;
	assert(timespec_get(&now, TIME_UTC) == TIME_UTC);
	assert(now.tv_nsec >= 0);
	assert(now.tv_nsec < 1000000000);

	struct timespec timeout_prev = now;
	struct atem_packet* packet_prev = NULL;
	while (packet != NULL) {
		atem_assert_packet(packet);

		assert(packet->timeout.tv_nsec >= 0);
		assert(packet->timeout.tv_nsec < 1000000000);

		// Asserts timeout is within expected range
		time_t timeout_remaining = (packet->timeout.tv_sec - now.tv_sec) * 1000;
		timeout_remaining += (time_t)((packet->timeout.tv_nsec - now.tv_nsec) / 1000000);
		assert(timeout_remaining <= atem_server.retransmit_delay);
		timeout_prev = packet->timeout;

		assert(packet->prev == packet_prev);
		packet_prev = packet;
		packet = packet->next;
	}
	assert(packet == NULL);
	assert(atem_server.packet_queue_tail == packet_prev);
}

// Asserts a specified connected session along with its packet chain
void atem_assert_session_connected(int16_t session_index) {
	struct atem_session* session = atem_session_get(session_index);
	assert(session->peer_addr.sin_family == AF_INET);

	// Asserts sessions session id
	assert(session->session_id & 0x8000);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	uint16_t request_session_id = (session->session_id_high << 8 | session->session_id_low) & 0xffff;
	assert(request_session_id == session->session_id);

	// Asserts sessions packet chain
	struct atem_packet* packet = session->packet_head;
	uint16_t packet_session_index = session->packet_session_index_head;
	if (packet == NULL) return;
	assert(session->packet_tail != NULL);
	struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
	uint16_t remoteid_next = (packet_session->remote_id_high << 8 | packet_session->remote_id_low) & 0xffff;
	uint16_t packet_session_index_last;
	struct atem_packet* packet_last;
	assert(packet != NULL);
	do {
		assert(!(packet->buf[ATEM_INDEX_FLAGS] & ATEM_FLAG_SYN));
		assert(!(packet->flags & ATEM_PACKET_FLAG_CLOSING));
		assert(packet->resends_remaining >= 0);
		assert(packet->sessions_remaining > 0);
		assert(packet_session_index < packet->sessions_len);
		packet_session = atem_packet_session_get(packet, packet_session_index);
		assert(packet_session->session_id == session->session_id);
		uint16_t remoteid = (packet_session->remote_id_high << 8 | packet_session->remote_id_low) & 0xffff;
		assert(!(remoteid & 0x8000));
		assert(remoteid_next == remoteid);

		// Asserts packet exists in global packet queue
		struct atem_packet* packet_queue = atem_server.packet_queue_head;
		while (packet_queue != packet) {
			assert(packet_queue != NULL);
			packet_queue = packet_queue->next;
		}

		remoteid_next = remoteid + 1;
		packet_last = packet;
		packet_session_index_last = packet_session_index;
		packet_session_index = packet_session->packet_session_index_next;
		packet = packet_session->packet_next;
	} while (packet != NULL);
	assert(packet == NULL);
	assert(session->packet_tail == packet_last);
	assert(session->packet_session_index_tail == packet_session_index_last);
	assert(session->remote_id == remoteid_next - 1);
}

// Asserts a specified opening or closing session along with its single packet
void atem_assert_session_unconnected(int16_t session_index) {
	struct atem_session* session = atem_session_get(session_index);

	// Asserts sessions session id
	assert(session->session_id & 0x8000);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	uint16_t request_session_id = (session->session_id_high << 8 | session->session_id_low) & 0xffff;
	assert(atem_session_lookup_get(request_session_id) == session_index);

	// Asserts sessions single packet
	struct atem_packet* packet = session->packet_head;
	assert(packet != NULL);
	assert(packet == session->packet_tail);
	assert(packet->resends_remaining >= 0);
	assert(packet->sessions_remaining > 0);
	struct atem_packet_session* packet_session = atem_packet_session_get(packet, session->packet_session_index_head);
	assert(packet_session->session_id == session->session_id);
	assert(packet_session->packet_next == NULL);
	uint16_t len = (packet->buf[ATEM_INDEX_LEN_HIGH] << 8 | packet->buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;
	assert(len == ATEM_LEN_SYN);

	// Asserts closing handshake
	if (packet->flags & ATEM_PACKET_FLAG_CLOSING) {
		assert(request_session_id == session->session_id);
		assert(request_session_id & 0x8000);
	}
	// Asserts opening handshake
	else {
		assert(request_session_id != session->session_id);
		assert(!(request_session_id & 0x8000));
	}
}

// Asserts all sessions from sessions list and from lookup table to ensure there are no double links
void atem_assert_sessions(void) {
	assert(atem_server.sessions != NULL);
	assert(atem_server.sessions_connected <= atem_server.sessions_len);
	assert(atem_server.sessions_len <= atem_server.sessions_size);
	assert(atem_server.sessions_len <= atem_server.sessions_limit);

	// Asserts all connected sessions from sessions list
	for (uint16_t i = 0; i < atem_server.sessions_connected; i++) {
		assert(i < INT16_MAX);
		atem_assert_session_connected((int16_t)i);
	}

	// Asserts all connected sessions from lookup table
	for (uint16_t id = 0xffff; id > 0x7fff; id--) {
		assert(id & 0x8000);
		int16_t session_index = atem_session_lookup_get(id);
		if (session_index != -1) {
			assert(session_index >= 0);
			assert(session_index < atem_server.sessions_len);
			struct atem_session* session = atem_session_get(session_index);
			assert(session->session_id == id);
			assert(atem_session_lookup_get(session->session_id) == session_index);
		}
	}

	// Asserts all opening and closing session from sessions list
	for (uint16_t i = atem_server.sessions_connected; i < atem_server.sessions_len; i++) {
		assert(i < INT16_MAX);
		atem_assert_session_unconnected((int16_t)i);
	}

	// Asserts all opening and closing sessions from lookup table
	for (uint16_t id = 0; id < 0x8000; id++) {
		assert(!(id & 0x8000));
		int16_t session_index = atem_session_lookup_get(id);
		if (session_index != -1) {
			assert(session_index >= atem_server.sessions_connected);
			assert(session_index < atem_server.sessions_len);
			struct atem_session* session = atem_session_get(session_index);
			assert((session->session_id_high << 8 | session->session_id_low) == id);
			assert(atem_session_lookup_get(session->session_id) == session_index);
		}
	}
}

// Asserts server data structure
void atem_assert(void) {
	assert(atem_server.sessions != NULL);
	assert(!(atem_server.session_id_last & 0x8000));

	assert(atem_server.sessions_connected <= atem_server.sessions_len);
	assert(atem_server.sessions_len <= atem_server.sessions_size);
	assert(atem_server.sessions_len <= atem_server.sessions_limit);
	assert(atem_server.session_id_last < 0x8000);

	assert(atem_server.retransmit_delay > 0);
	assert(atem_server.retransmit_delay > 0);
	assert(atem_server.ping_interval > atem_server.retransmit_delay);

	if (atem_server.sessions_connected > 0) {
		struct timespec now;
		assert(timespec_get(&now, TIME_UTC) == TIME_UTC);
		assert(now.tv_nsec >= 0);
		assert(now.tv_nsec < 1000000000);
		assert(atem_server.ping_timeout.tv_nsec >= 0);
		assert(atem_server.ping_timeout.tv_nsec < 1000000000);

		// Asserts ping timeout is within the expected range
		time_t timeout_remaining = (atem_server.ping_timeout.tv_sec - now.tv_sec) * 1000;
		timeout_remaining += (time_t)((atem_server.ping_timeout.tv_nsec - now.tv_nsec) / 1000000);
		assert(timeout_remaining <= atem_server.ping_interval);
	}

	atem_assert_sessions();
	atem_assert_packets();
}

#endif // NDEBUG
