#include <stddef.h> // size_t, NULL
#include <stdlib.h> // malloc, free, abort
#include <stdint.h> // uint8_t, uint16_t, int16_t
#include <time.h> // timespec_get, TIME_UTC
#include <assert.h> // assert
#include <stdbool.h> // true, false
#include <string.h> // memset

#include "../core/atem_protocol.h" // ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, ATEM_LEN_HEADER, ATEM_INDEX_FLAGS, ATEM_FLAG_RETX, ATEM_LEN_SYN, ATEM_FLAG_SYN, ATEM_INDEX_LEN_LOW, ATEM_INDEX_OPCODE, ATEM_OPCODE_CLOSING, ATEM_RESENDS_CLOSING
#include "./atem_server.h" // atem_server, atem_server_release
#include "./atem_session.h" // struct atem_session, atem_session_send, atem_session_get, atem_session_lookup_get, atem_session_release, atem_session_terminate
#include "./atem_packet.h" // struct atem_packet_session, struct atem_packet
#include "./atem_debug.h" // DEBUG_PRINTF

// Preallocated closing request buffer
static uint8_t buf_closing[ATEM_LEN_SYN] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
	[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSING
};

// Preallocated ping buffer
static uint8_t buf_ping[ATEM_LEN_HEADER] = {
	[ATEM_INDEX_LEN_LOW] = ATEM_LEN_HEADER
};



// Requeues packet at the top of the queue to the end and updates its timestamp
static void atem_packet_requeue(void) {
	struct atem_packet* packet = atem_server.packet_queue_head;
	assert(packet != NULL);

	DEBUG_PRINTF("Requeueing packet %p\n", (void*)packet);

	// Moves the packet to the end of the global packet queue
	if (atem_server.packet_queue_tail != packet) {
		// Updates head of queue to point to next packet in line
		atem_server.packet_queue_head = packet->next;
		atem_server.packet_queue_head->prev = NULL;

		// Updates tail of queue to point to packet that was requeued
		packet->prev = atem_server.packet_queue_tail;
		packet->next = NULL;
		atem_server.packet_queue_tail->next = packet;
		atem_server.packet_queue_tail = packet;
	}

	// Ensures packet is correctly placed at the end of the global packet queue
	assert(atem_server.packet_queue_head != NULL);
	assert(atem_server.packet_queue_head->prev == NULL);
	assert(atem_server.packet_queue_tail == packet);
	assert(atem_server.packet_queue_tail->next == NULL);
	if (atem_server.packet_queue_head != atem_server.packet_queue_tail) {
		assert(atem_server.packet_queue_head->next != NULL);
		assert(atem_server.packet_queue_head->next->prev == atem_server.packet_queue_head);
		assert(atem_server.packet_queue_tail->prev != NULL);
		assert(atem_server.packet_queue_tail->prev->next == atem_server.packet_queue_tail);
	}

	// Updates timeout to time of requeuing
	int timespec_result = timespec_get(&packet->timeout, TIME_UTC);
	assert(timespec_result == TIME_UTC);
}



// Gets packets packet session at specified index
struct atem_packet_session* atem_packet_session_get(struct atem_packet* packet, uint16_t packet_session_index) {
	assert(packet != NULL);
	assert(packet_session_index < packet->sessions_len);
	return &packet->sessions[packet_session_index];
}



// Creates an ATEM packet, ready to be sent to sessions and enqueued. Buffer will be released along with packet
struct atem_packet* atem_packet_create(uint8_t* buf, uint16_t sessions_count) {
	assert(buf != NULL);
	assert(sessions_count > 0);
	assert(sessions_count < INT16_MAX);
	assert(sessions_count <= atem_server.sessions_len);

	uint16_t len = (buf[ATEM_INDEX_LEN_HIGH] << 8 | buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;
	assert(len >= ATEM_LEN_HEADER);
	assert(len <= ATEM_PACKET_LEN_MAX);

	// Allocates memory for ATEM packet with space for specified number of sessions to link to
	size_t packet_session_size = sizeof(struct atem_packet_session) * sessions_count;
	struct atem_packet* packet = malloc(sizeof(struct atem_packet) + packet_session_size);
	if (packet == NULL) {
		perror("Failed to allocate packet manager memory\n");
		abort(); // @todo could probably return NULL instead and handle errors in the caller
	}

	DEBUG_PRINTF("Creating packet %p\n", (void*)packet);

	// Initializes packet memory
	packet->buf = buf;
	packet->sessions_remaining = sessions_count;
	#ifndef NDEBUG
	packet->sessions_len = sessions_count;
	#endif // NDEBUG

	return packet;
}

// Transmits the ATEM packet to the session peer
void atem_packet_send(struct atem_packet* packet, struct atem_packet_session* packet_session) {
	assert(packet != NULL);
	assert(packet_session != NULL);
	struct atem_session* session = atem_session_get(atem_session_lookup_get(packet_session->session_id));
	assert(session != NULL);
	assert(session->session_id == packet_session->session_id);

	packet->buf[ATEM_INDEX_REMOTEID_HIGH] = packet_session->remote_id_high;
	packet->buf[ATEM_INDEX_REMOTEID_LOW] = packet_session->remote_id_low;
	atem_session_send(session, packet->buf);
}

// Enqueues packet to ATEM server for retransmission
void atem_packet_enqueue(struct atem_packet* packet, uint8_t flags) {
	assert(packet != NULL);
	assert(packet->buf != NULL);
	assert(packet->sessions_remaining <= atem_server.sessions_len);

	DEBUG_PRINTF("Enqueueing packet %p\n", (void*)packet);

	packet->flags = flags;
	packet->resends_remaining = ATEM_RESENDS;
	int timespec_result = timespec_get(&packet->timeout, TIME_UTC);
	assert(timespec_result == TIME_UTC);

	if (atem_server.packet_queue_head == NULL) {
		atem_server.packet_queue_head = packet;
		packet->prev = NULL;
	}
	else {
		atem_server.packet_queue_tail->next = packet;
		packet->prev = atem_server.packet_queue_tail;
	}
	atem_server.packet_queue_tail = packet;
	packet->next = NULL;
}

// Removes packet from server packet queue
void atem_packet_dequeue(struct atem_packet* packet) {
	assert(packet != NULL);
	assert(atem_server.packet_queue_head != NULL);
	assert(atem_server.packet_queue_head->prev == NULL);
	assert(atem_server.packet_queue_tail != NULL);
	assert(atem_server.packet_queue_tail->next == NULL);

	// Removes packet from packet queue
	if (packet->prev == NULL) {
		assert(atem_server.packet_queue_head == packet);
		atem_server.packet_queue_head = packet->next;
	}
	else {
		assert(atem_server.packet_queue_head != packet);
		packet->prev->next = packet->next;
	}
	if (packet->next == NULL) {
		assert(atem_server.packet_queue_tail == packet);
		atem_server.packet_queue_tail = packet->prev;
	}
	else {
		assert(atem_server.packet_queue_tail != packet);
		packet->next->prev = packet->prev;
	}
	if (atem_server.packet_queue_head != NULL) {
		assert(atem_server.packet_queue_head->prev == NULL);
		assert(atem_server.packet_queue_tail->next == NULL);
	}
}

// Releases packet from packet queue and its memory
void atem_packet_release(struct atem_packet* packet) {
	atem_packet_dequeue(packet);

	DEBUG_PRINTF("Releasing packet: %p\n", (void*)packet);

	// Releases packets memory
	if (packet->flags & ATEM_PACKET_FLAG_RELEASE) {
		free(packet->buf);
	}
	free(packet);
}

// Disassociates a packet from a session
void atem_packet_disassociate(struct atem_packet* packet, uint16_t packet_session_index) {
	assert(packet != NULL);
	assert(packet_session_index < packet->sessions_len);

	struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
	DEBUG_PRINTF(
		"Disassociates packet %p from session 0x%04x\n",
		(void*)packet,
		packet_session->session_id
	);

	// Disassociates session from packet if there are other sessions still associated to it
	// Reduces bounds of sessions array and points vacated slot to last packet session to
	// easily iterate through all remaining sessions for retransmission
	if (packet->sessions_remaining > 1) {
		packet->sessions_remaining--;
		uint16_t packet_session_index_last = packet->sessions[packet->sessions_remaining].packet_session_index;
		packet->sessions[packet_session_index_last].packet_session_index = packet_session->packet_session_index;
		packet->sessions[packet_session->packet_session_index].packet_session_index = packet_session_index_last;
	}
	// Releases packet when all sessions have been disassociated from it
	else {
		assert(packet->sessions_remaining == 1);
		atem_packet_release(packet);
	}
}

// Flushes all subsequent packets from session starting with specified packet
void atem_packet_flush(struct atem_packet* packet, uint16_t packet_session_index) {
	while (packet != NULL) {
		assert(!(packet->flags & ATEM_PACKET_FLAG_CLOSING));
		assert(packet_session_index < packet->sessions_len);
		struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
		assert(packet_session != NULL);
		struct atem_packet* packet_next = packet_session->packet_next;
		uint16_t packet_session_index_next = packet_session->packet_session_index_next;
		atem_packet_disassociate(packet, packet_session_index);
		packet = packet_next;
		packet_session_index = packet_session_index_next;
	}
}

// Retransmits oldest ATEM packet or drops sessions not having acknowledged it after retransmits run out
void atem_packet_retransmit(void) {
	struct atem_packet* packet = atem_server.packet_queue_head;
	assert(packet != NULL);
	assert(packet->prev == NULL);

	// Sends packet to sessions as retransmit if there are retransmits left
	if (packet->resends_remaining > 0) {
		assert(packet == atem_server.packet_queue_head);
		packet->buf[ATEM_INDEX_FLAGS] |= ATEM_FLAG_RETX;
		for (uint16_t i = 0; i < packet->sessions_remaining; i++) {
			uint16_t packet_session_index = packet->sessions[i].packet_session_index;
			struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
			DEBUG_PRINTF(
				"Retransmitting packet 0x%02x%02x (%p) for session 0x%04x\n",
				packet_session->remote_id_high, packet_session->remote_id_low,
				(void*)packet,
				packet_session->session_id
			);
			atem_packet_send(packet, packet_session);
		}
		atem_packet_requeue();
		packet->resends_remaining--;
		return;
	}

	// Terminates sessions not having acknowledged closing request sent after retransmits ran out
	if (packet->flags & ATEM_PACKET_FLAG_CLOSING) {
		// Removes sessions from server
		for (uint16_t i = 0; i < packet->sessions_remaining; i++) {
			uint16_t packet_session_index = packet->sessions[i].packet_session_index;
			struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
			int16_t session_index = atem_session_lookup_get(packet_session->session_id);
			assert(session_index >= atem_server.sessions_connected);
			struct atem_session* session = atem_session_get(session_index);
			assert(atem_session_lookup_get(session->session_id) == session_index);
			assert(session->packet_head == packet);
			assert(session->packet_tail == packet);

			DEBUG_PRINTF("Dropping session 0x%04x\n", session->session_id);
			atem_session_terminate(session_index);
		}

		// Dequeues packet from servers packet queue
		assert(atem_server.packet_queue_head == packet);
		atem_server.packet_queue_head = packet->next;
		if (atem_server.packet_queue_head != NULL) {
			atem_server.packet_queue_head->prev = NULL;
		}

		DEBUG_PRINTF("Releases packet on close %p\n", (void*)packet);

		// Releases packet memory without releasing preallocated buffer
		free(packet);

		return;
	}

	// Replaces packet buffer with preallocated closing request
	if (packet->flags & ATEM_PACKET_FLAG_RELEASE) {
		free(packet->buf);
	}
	buf_closing[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	packet->buf = buf_closing;

	// Starts closing sessions when packet has run out of retransmits
	for (uint16_t i = 0; i < packet->sessions_remaining; i++) {
		uint16_t packet_session_index = packet->sessions[i].packet_session_index;
		struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index);
		uint16_t session_index = atem_session_lookup_get(packet_session->session_id);
		struct atem_session* session = atem_session_get(session_index);
		assert(atem_session_lookup_get(session->session_id) == session_index);
		assert(session->packet_session_index_head == packet_session_index);
		assert(session->packet_head == packet);

		// Disassociates all packets from the session other than the closing request
		atem_packet_flush(packet_session->packet_next, packet_session->packet_session_index_next);
		session->packet_tail = session->packet_head;

		// Evicts session out after connected sessions in list
		atem_session_drop(session_index);

		// Sets closing packet to be sessions only packet after potential session index swap
		packet_session->packet_next = NULL;
		packet_session->remote_id_high = 0;
		packet_session->remote_id_low = 0;

		// Sends closing request to session
		atem_packet_send(packet, packet_session);
	}

	// Re-initializes packet for closing request
	packet->resends_remaining = ATEM_RESENDS_CLOSING;
	packet->flags = ATEM_PACKET_FLAG_CLOSING;
	assert(packet == atem_server.packet_queue_head);
	atem_packet_requeue();
}



// Sends closing request to all sessions, assumes existing packets has been flushes and sessions ready for closing
void atem_packet_broadcast_close(void) {
	assert(atem_server.sessions_connected == 0);
	assert(atem_server.sessions_len > 0);
	assert(atem_server.packet_queue_head == NULL);
	assert(atem_server.packet_queue_tail == NULL);

	// Creates, broadcasts and enqueues closing handshake packet
	buf_closing[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	struct atem_packet* packet = atem_packet_create(buf_closing, atem_server.sessions_len);
	for (int16_t session_index = atem_server.sessions_len - 1; session_index >= 0; session_index--) {
		struct atem_session* session = atem_session_get(session_index);
		assert(atem_session_lookup_get(session->session_id) == session_index);
		assert((session->session_id_high << 8 | session->session_id_low) == session->session_id);
		session->packet_head = packet;
		session->packet_tail = packet;
		session->packet_session_index_head = session_index;

		struct atem_packet_session* packet_session = atem_packet_session_get(packet, session_index);
		packet_session->session_id = session->session_id;
		packet_session->packet_next = NULL;
		packet_session->packet_session_index = session_index;
		packet_session->remote_id_high = 0;
		packet_session->remote_id_low = 0;

		atem_session_send(session, packet->buf);
	}
	atem_packet_enqueue(packet, ATEM_PACKET_FLAG_CLOSING);
	assert(atem_server.packet_queue_head == packet);
	assert(atem_server.packet_queue_tail == packet);
}

// Pings all connected sessions
void atem_packet_broadcast_ping(void) {
	assert(atem_server.sessions_connected > 0);

	DEBUG_PRINTF("Pings all %d connected clients\n", atem_server.sessions_connected);
	buf_ping[ATEM_INDEX_FLAGS] = ATEM_FLAG_ACKREQ;
	atem_server_broadcast(buf_ping, 0);

	// Sets timeout for next ping
	int timespec_result = timespec_get(&atem_server.ping_timeout, TIME_UTC);
	assert(timespec_result == TIME_UTC);
}
