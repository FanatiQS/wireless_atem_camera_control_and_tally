#include <stdint.h> // uint8_t, uint16_t, int16_t
#include <stddef.h> // NULL, size_t
#include <assert.h> // assert
#include <stdio.h> // perror
#include <stdlib.h> // malloc, realloc, abort
#include <stdbool.h> // bool, true, false
#include <string.h> // memset
#include <time.h> // timespec_get, TIME_UTC

#include <sys/socket.h> // AF_INET, sendto, struct sockaddr
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // ntohs
#include <unistd.h> // ssize_t

#include "../core/atem_protocol.h" // ATEM_LEN_HEADER, ATEM_INDEX_LEN_HIGH, ATEM_INDEX_LEN_LOW,ATEM_INDEX_SESSIONID_HIGH, ATEM_INDEX_SESSIONID_LOW, ATEM_INDEX_FLAGS, ATEM_FLAG_RETX, ATEM_FLAG_SYN, ATEM_LEN_SYN, ATEM_INDEX_OPCODE, ATEM_OPCODE_REJECT, ATEM_OPCODE_ACCEPT, ATEM_INDEX_NEWSESSIONID_HIGH, ATEM_INDEX_NEWSESSIONID_LOW, ATEM_OPCODE_CLOSED
#include "../core/atem.h" // ATEM_PACKET_LEN_MAX
#include "./atem_debug.h" // DEBUG_PRINTF, DEBUG_PRINT_BUF
#include "./atem_server.h" // atem_server, atem_server_release
#include "./atem_packet.h" // struct atem_packet, struct atem_packet_session, atem_packet_create, atem_packet_enqueue, atem_packet_release, atem_packet_session_update, atem_packet_disassociate, atem_packet_flush, ATEM_PACKET_FLAG_CLOSING
#include "./atem_cache.h" // atem_cache_dump
#include "./atem_session.h" // struct atem_session

// How much to grow the sessions array by when it runs out of slots
#define SESSIONS_ARRAY_MULTIPLIER 1.6f



// Sends an ATEM packet buffer to a specified client address
static void atem_send(uint8_t* buf, struct sockaddr_in* peer_addr) {
	assert(buf != NULL);
	assert(peer_addr != NULL);
	assert(peer_addr->sin_family == AF_INET);

	uint16_t len = (buf[ATEM_INDEX_LEN_HIGH] << 8 | buf[ATEM_INDEX_LEN_LOW]) & ATEM_PACKET_LEN_MAX;
	assert(len >= ATEM_LEN_HEADER);
	assert(len <= ATEM_PACKET_LEN_MAX);

	DEBUG_PRINTF("Sending: ");
	DEBUG_PRINT_BUF(buf, len);

	ssize_t sent = sendto(atem_server.sock, buf, (size_t)len, 0, (struct sockaddr*)peer_addr, sizeof(*peer_addr));
	if (sent == -1) {
		perror("Failed to send data to ATEM proxy client");
		return;
	}
	assert(sent == len);
}

// Sets session index for specified session id in lookup table
static inline void atem_session_lookup_set(uint16_t session_id, int16_t session_index) {
	assert(session_id < (sizeof(atem_server.session_lookup_table) / sizeof(*atem_server.session_lookup_table)));
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);
	assert(session_index < atem_server.sessions_size);
	atem_server.session_lookup_table[session_id] = session_index + 1;
}

// Figures out if a session is connected or not based on its index
static inline bool atem_session_connected(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);
	assert(session_index < atem_server.sessions_size);
	return session_index < atem_server.sessions_connected;
}

/**
 * Swaps session at session_index with first session after connected sessions in memory, including session id
 * @attention Does not update potential request session id for either session
 * @attention Should only swap session slots if required
 */
static void atem_session_swap(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);
	assert(session_index != atem_server.sessions_connected);
	DEBUG_PRINTF(
		"Swapping session slots: %d (0x%04x), %d (0x%04x)\n",
		atem_server.sessions_connected, atem_session_get(atem_server.sessions_connected)->session_id,
		session_index, atem_session_get(session_index)->session_id
	);

	// Swaps session memory at specified session index and first session after connected sessions section
	struct atem_session* session_a = atem_session_get(session_index);
	struct atem_session* session_b = atem_session_get(atem_server.sessions_connected);
	struct atem_session tmp_session = *session_a;
	*session_a = *session_b;
	*session_b = tmp_session;

	// Updates lookup table session indexes for both sessions
	assert(atem_session_lookup_get(session_a->session_id) == atem_server.sessions_connected);
	assert(atem_session_lookup_get(session_b->session_id) == session_index);
	atem_session_lookup_set(session_a->session_id, session_index);
	atem_session_lookup_set(session_b->session_id, atem_server.sessions_connected);
}

/**
 * Removes session from sessions array after it has been unregistered from the lookup table
 * @attention Might move sessions in memory, invalidating all session pointers
 */
static void atem_session_release(int16_t session_index) {
	assert(session_index >= atem_server.sessions_connected);
	assert(session_index < atem_server.sessions_len);
	assert(session_index < atem_server.sessions_size);

	// Moves last session into released slot
	atem_server.sessions_len--;
	if (session_index != atem_server.sessions_len) {
		struct atem_session* session = atem_session_get(session_index);
		assert(session != NULL);

		DEBUG_PRINTF(
			"Moving session %d (0x%04x) to %d (0x%04x)\n",
			atem_server.sessions_len, atem_session_get(atem_server.sessions_len)->session_id,
			session_index, session->session_id
		);

		*session = *atem_session_get(atem_server.sessions_len);
		assert(atem_session_lookup_get(session->session_id) == atem_server.sessions_len);
		uint16_t request_session_id_moved = session->session_id_high << 8 | session->session_id_low;
		assert(atem_session_lookup_get(request_session_id_moved) == atem_server.sessions_len);
		atem_session_lookup_set(session->session_id, session_index);
		atem_session_lookup_set(request_session_id_moved, session_index);
	}

	// Reduces session slots when size is more than 1 multiplication away
	const float multiplier_resize = 1.0f / SESSIONS_ARRAY_MULTIPLIER;
	const float multiplier_limit = multiplier_resize / SESSIONS_ARRAY_MULTIPLIER;
	uint16_t sessions_size_resize_limit = (uint16_t)(atem_server.sessions_size * multiplier_limit);
	if (atem_server.sessions_len < sessions_size_resize_limit && sessions_size_resize_limit > 1) {
		atem_server.sessions_size = (uint16_t)(atem_server.sessions_size * multiplier_resize);
		DEBUG_PRINTF("Reducing sessions memory to %d\n", atem_server.sessions_size);

		assert(atem_server.sessions != NULL);
		struct atem_session* sessions = realloc(
			atem_server.sessions,
			sizeof(*atem_server.sessions) * atem_server.sessions_size
		);
		if (sessions == NULL) {
			perror("Failed to shrink sessions list size, keeps old allocation");
		}
		else {
			atem_server.sessions = sessions;
		}
	}
}



// Gets session index for specified session id in lookup table
int16_t atem_session_lookup_get(uint16_t session_id) {
	assert(session_id < (sizeof(atem_server.session_lookup_table) / sizeof(*atem_server.session_lookup_table)));
	return atem_server.session_lookup_table[session_id] - 1;
}

// Clears session id lookup
void atem_session_lookup_clear(uint16_t session_id) {
	assert(session_id < (sizeof(atem_server.session_lookup_table) / sizeof(*atem_server.session_lookup_table)));
	atem_server.session_lookup_table[session_id] = 0;
}

// Gets session pointer from session index
struct atem_session* atem_session_get(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_size);
	assert(session_index <= atem_server.sessions_len);
	struct atem_session* session = &atem_server.sessions[session_index];
	return session;
}

// Validates that request comes from same peer as the one that created the session
bool atem_session_peer_validate(struct atem_session* session, struct sockaddr_in* peer_addr) {
	assert(session != NULL);
	assert(peer_addr != NULL);
	assert(session->peer_addr.sin_family == AF_INET);
	assert(peer_addr->sin_family == AF_INET);

	// Ensures that both address and port in peer_addr match what is stored in session
	bool cmp_addr = (session->peer_addr.sin_addr.s_addr == peer_addr->sin_addr.s_addr);
	bool cmp_port = (session->peer_addr.sin_port == peer_addr->sin_port);
	if (!cmp_addr || !cmp_port) {
		DEBUG_PRINTF("Rejected packet from wrong peer\n");
		return false;
	}
	return true;
}

// Sends ATEM packet to session
void atem_session_send(struct atem_session* session, uint8_t* buf) {
	assert(session != NULL);
	assert(buf != NULL);

	buf[ATEM_INDEX_SESSIONID_HIGH] = session->session_id_high;
	buf[ATEM_INDEX_SESSIONID_LOW] = session->session_id_low;
	atem_send(buf, &session->peer_addr);
}



// Creates ATEM session from opening handshake request
void atem_session_create(uint8_t session_id_high, uint8_t session_id_low, struct sockaddr_in* peer_addr) {
	assert(peer_addr != NULL);

	// Retransmits response to opening handshake that is already in progress
	uint16_t request_session_id = session_id_high << 8 | session_id_low;
	assert(request_session_id < 0x8000);
	int16_t session_index = atem_session_lookup_get(request_session_id);
	if (session_index != -1) {
		assert(session_index >= atem_server.sessions_connected);
		assert(session_index < atem_server.sessions_len);
		struct atem_session* session = atem_session_get(session_index);
		assert(session != NULL);
		assert(atem_session_lookup_get(session->session_id) == session_index);
		if (!atem_session_peer_validate(session, peer_addr)) return;

		struct atem_packet* packet = session->packet_head;
		assert(packet != NULL);
		assert(session->packet_tail == packet);
		assert(session->packet_session_index_head == 0);
		assert(packet->flags == ATEM_PACKET_FLAG_RELEASE);
		assert(packet->sessions_remaining == 1);
		assert(packet->sessions[0].packet_next == NULL);
		assert(packet->sessions[0].session_id == session->session_id);
		assert(packet->sessions[0].packet_session_index == 0);
		assert(packet->sessions[0].remote_id_high == 0);
		assert(packet->sessions[0].remote_id_low == 0);
		packet->buf[ATEM_INDEX_FLAGS] |= ATEM_FLAG_RETX;
		atem_session_send(session, packet->buf);
		return;
	}

	// Rejects session if there are no slots available
	assert(atem_server.sessions_len <= atem_server.sessions_limit);
	if ((atem_server.sessions_len == atem_server.sessions_limit) || atem_server.closing) {
		uint8_t response_reject[ATEM_LEN_SYN] = {
			[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
			[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
			[ATEM_INDEX_SESSIONID_HIGH] = session_id_high,
			[ATEM_INDEX_SESSIONID_LOW] = session_id_low,
			[ATEM_INDEX_OPCODE] = ATEM_OPCODE_REJECT
		};
		atem_send(response_reject, peer_addr);
		return;
	}

	// Assigns empty slot in server sessions array to created session
	session_index = atem_server.sessions_len++;
	assert(session_index <= atem_server.sessions_size);
	if (session_index == atem_server.sessions_size) {
		atem_server.sessions_size *= SESSIONS_ARRAY_MULTIPLIER;
		if (atem_server.sessions_size > atem_server.sessions_limit) {
			atem_server.sessions_size = atem_server.sessions_limit;
		}
		assert(session_index < atem_server.sessions_size);
		DEBUG_PRINTF("Expanding sessions memory to %d slots\n", atem_server.sessions_size);
		assert(atem_server.sessions != NULL);
		atem_server.sessions = realloc(
			atem_server.sessions,
			sizeof(*atem_server.sessions) * atem_server.sessions_size
		);
		if (atem_server.sessions == NULL) {
			perror("Failed to grow sessions list");
			abort();
		}
	}
	struct atem_session* session = atem_session_get(session_index);
	session->remote_id = 0;
	session->peer_addr = *peer_addr;
	session->session_id_high = session_id_high;
	session->session_id_low = session_id_low;

	// Assigns session id to session
	atem_server.session_id_last++;
	atem_server.session_id_last &= 0x7fff;
	session->session_id = atem_server.session_id_last | 0x8000;
	assert(session->session_id <= 0xffff);
	assert(session->session_id >= 0x8000);

	// Registers both client assigned session id and server assigned session id in lookup table
	assert(atem_session_lookup_get(request_session_id) == -1);
	assert(atem_session_lookup_get(session->session_id) == -1);
	atem_session_lookup_set(request_session_id, session_index);
	atem_session_lookup_set(session->session_id, session_index);

	DEBUG_PRINTF(
		"Creating session 0x%04x (0x%04x) from %d.%d.%d.%d:%d\n",
		session->session_id, request_session_id,
		peer_addr->sin_addr.s_addr & 0xff, (peer_addr->sin_addr.s_addr >> 8) & 0xff,
		(peer_addr->sin_addr.s_addr >> 16) & 0xff, (peer_addr->sin_addr.s_addr >> 24) & 0xff,
		ntohs(peer_addr->sin_port)
	);

	// Sends accept response
	uint8_t* buf_accept = malloc(ATEM_LEN_SYN);
	if (buf_accept == NULL) {
		perror("Failed to allocate packet memory");
		abort();
	}
	memset(buf_accept, 0, ATEM_LEN_SYN);
	buf_accept[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN;
	buf_accept[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN;
	buf_accept[ATEM_INDEX_OPCODE] = ATEM_OPCODE_ACCEPT;
	buf_accept[ATEM_INDEX_NEWSESSIONID_HIGH] = atem_server.session_id_last >> 8;
	buf_accept[ATEM_INDEX_NEWSESSIONID_LOW] = atem_server.session_id_last & 0xff;
	atem_session_send(session, buf_accept);

	// Pushes packet to retransmit queue
	struct atem_packet* packet = atem_packet_create(buf_accept, 1);
	assert(packet != NULL);
	atem_packet_enqueue(packet, ATEM_PACKET_FLAG_RELEASE);
	session->packet_head = packet;
	session->packet_tail = packet;
	session->packet_session_index_head = 0;
	struct atem_packet_session* packet_session = &packet->sessions[0];
	packet_session->packet_next = NULL;
	packet_session->session_id = session->session_id;
	packet_session->packet_session_index = 0;
	packet_session->remote_id_high = 0;
	packet_session->remote_id_low = 0;
}

// Completes ATEM session opening handshake
void atem_session_connect(uint8_t session_id_high, uint8_t session_id_low, struct sockaddr_in* peer_addr) {
	assert(peer_addr != NULL);

	// Ensures requested session to connect exists
	uint16_t request_session_id = session_id_high << 8 | session_id_low;
	assert(request_session_id < 0x8000);
	int16_t session_index = atem_session_lookup_get(request_session_id);
	if (session_index == -1) {
		DEBUG_PRINTF("Session does not exist for opening handshake completion 0x%04x\n", request_session_id);
		return;
	}

	// Rejects completion if request comes from different peer than request did
	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	if (!atem_session_peer_validate(session, peer_addr)) {
		DEBUG_PRINTF("Completion request from another peer rejected\n");
		return;
	}

	// Moves session into connected sessions segment by swapping session slots
	assert(atem_session_connected(session_index) == false);
	assert(session_index >= atem_server.sessions_connected);
	if (session_index != atem_server.sessions_connected) {
		atem_session_swap(session_index);

		// Updates potential request session id for moved session
		uint16_t request_session_id_moved = session->session_id_high << 8 | session->session_id_low;
		if (request_session_id_moved == session->session_id) {
			assert(atem_session_lookup_get(request_session_id_moved) == session_index);
		}
		else {
			assert(atem_session_lookup_get(request_session_id_moved) == atem_server.sessions_connected);
		}
		atem_session_lookup_set(request_session_id_moved, session_index);

		// Updates pointer to connected session after swap
		session = atem_session_get(atem_server.sessions_connected);
		assert(session != NULL);
		assert(atem_session_lookup_get(session->session_id) == atem_server.sessions_connected);
	}
	assert(session->session_id_high == session_id_high);
	assert(session->session_id_low == session_id_low);

	// Enables ping interval timer if no sessions were connected before this one
	if (atem_server.sessions_connected == 0) {
		int timespec_result = timespec_get(&atem_server.ping_timeout, TIME_UTC);
		assert(timespec_result == TIME_UTC);
	}

	// Fully connects session by deprecating client assigned session id to enable broadcasts for session
	atem_server.sessions_connected++;
	assert(atem_session_lookup_get(request_session_id) == session_index);
	atem_session_lookup_clear(request_session_id);
	session->session_id_high = session->session_id >> 8;
	session->session_id_low = session->session_id & 0xff;

	// Releases handshake accept packet
	struct atem_packet* packet = session->packet_head;
	assert(packet != NULL);
	assert(session->packet_tail == packet);
	assert(session->packet_session_index_head == 0);
	assert(packet->flags == ATEM_PACKET_FLAG_RELEASE);
	assert(packet->sessions_remaining == 1);
	assert(packet->sessions[0].packet_next == NULL);
	assert(packet->sessions[0].session_id == session->session_id);
	assert(packet->sessions[0].packet_session_index == 0);
	assert(packet->sessions[0].remote_id_high == 0);
	assert(packet->sessions[0].remote_id_low == 0);

	DEBUG_PRINTF("Session connected 0x%04x\n", session->session_id);

	// Dumps cached state to client
	atem_cache_dump(session, packet);
}



// Initializes closing of session at index that can be either connected or not
void atem_session_drop(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);

	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	assert(atem_session_lookup_get(session->session_id_high << 8 | session->session_id_low) == session_index);

	struct atem_packet* packet = session->packet_head;
	assert(session->packet_tail == packet);
	uint16_t packet_session_index = session->packet_session_index_head;
	assert(atem_packet_session_get(packet, packet_session_index)->session_id == session->session_id);

	DEBUG_PRINTF("Dropping session 0x%04x\n", session->session_id);

	// Moves session to slot outside connected sessions to remove from broadcast targets if session is connected
	if (atem_session_connected(session_index)) {
		atem_server.sessions_connected--;
		if (session_index != atem_server.sessions_connected) {
			atem_session_swap(session_index);
		}
	}
	// Cleans up opening handshake client session id if session is not connected
	else {
		assert(packet->sessions_remaining == 1);
		assert(packet->flags == ATEM_PACKET_FLAG_RELEASE);
		uint16_t request_session_id = session->session_id_high << 8 | session->session_id_low;
		assert(request_session_id != session->session_id);
		assert(atem_session_lookup_get(request_session_id) == session_index);
		atem_session_lookup_clear(request_session_id);

		// Uses server assigned session id for closing handshake when closed during opening handshake
		session->session_id_high = session->session_id >> 8;
		session->session_id_low = session->session_id & 0xff;
	}
}

/**
 * Closes session at index that is not in a connected state
 * @attention Terminating sessions packet(s) should be taken care of before
 * @attention Unregistering of potential request session id should be taken care of before
 */
void atem_session_terminate(int16_t session_index) {
	assert(session_index >= atem_server.sessions_connected);
	assert(session_index < atem_server.sessions_len);
	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);

	DEBUG_PRINTF("Terminated session 0x%04x\n", session->session_id);

	// Unregisters server assigned session id from lookup table
	assert(atem_session_lookup_get(session->session_id) == session_index);
	atem_session_lookup_clear(session->session_id);

	// Releases session from sessions list
	atem_session_release(session_index);
}

// Completely closes session as response to client request
void atem_session_closing(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);
	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);

	DEBUG_PRINTF("Closing session: 0x%04x\n", session->session_id);

	// Sends closing response packet buffer
	uint8_t buf_closed[ATEM_LEN_SYN] = {
		[ATEM_INDEX_FLAGS] = ATEM_FLAG_SYN,
		[ATEM_INDEX_LEN_LOW] = ATEM_LEN_SYN,
		[ATEM_INDEX_SESSIONID_HIGH] = session->session_id >> 8,
		[ATEM_INDEX_SESSIONID_LOW] = session->session_id & 0xff,
		[ATEM_INDEX_OPCODE] = ATEM_OPCODE_CLOSED
	};
	atem_send(buf_closed, &session->peer_addr);

	// Closes session that is in the middle of an opening or closing handshake
	if (!atem_session_connected(session_index)) {
		// Disassociates from single opening or closing packet
		struct atem_packet* packet_head = session->packet_head;
		assert(session->packet_tail == packet_head);
		uint16_t packet_session_index_head = session->packet_session_index_head;
		struct atem_packet_session* packet_session = atem_packet_session_get(packet_head, packet_session_index_head);
		assert(packet_session->packet_next == NULL);
		assert(packet_session->session_id == session->session_id);
		assert(packet_session_index_head == atem_packet_session_get(packet_head, packet_session->packet_session_index)->packet_session_index);
		assert(packet_session->remote_id_high == 0);
		assert(packet_session->remote_id_low == 0);
		atem_packet_disassociate(packet_head, packet_session_index_head);

		uint16_t request_session_id = session->session_id_high << 8 | session->session_id_low;
		assert(atem_session_lookup_get(request_session_id) == session_index);
		atem_session_terminate(session_index);
		atem_session_lookup_clear(request_session_id);
		return;
	}

	// Unregisters server assigned session id from lookup table
	assert((session->session_id_high << 8 | session->session_id_low) == session->session_id);
	assert(atem_session_lookup_get(session->session_id) == session_index);
	atem_session_lookup_clear(session->session_id);

	// Disassociates session from all packets it is associated with
	atem_packet_flush(session->packet_head, session->packet_session_index_head);

	// Removes session from connected sessions part of array
	atem_server.sessions_connected--;
	if (session_index != atem_server.sessions_connected) {
		DEBUG_PRINTF(
			"Moving session %d (0x%04x) to %d (0x%04x)\n",
			atem_server.sessions_connected, atem_session_get(atem_server.sessions_connected)->session_id,
			session_index, session->session_id
		);
		*session = *atem_session_get(atem_server.sessions_connected);
		assert((session->session_id_high << 8 | session->session_id_low) == session->session_id);
		assert(atem_session_lookup_get(session->session_id) == atem_server.sessions_connected);
		atem_session_lookup_set(session->session_id, session_index);
	}

	// Releases session and moves in last session outside connected sessions into its place
	atem_session_release(atem_server.sessions_connected);
}

// Completes server initiated termination after client response
void atem_session_closed(int16_t session_index) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);
	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);

	// Rejects responses for never sent request
	struct atem_packet* packet_head = session->packet_head;
	if (packet_head == NULL || !(packet_head->flags & ATEM_PACKET_FLAG_CLOSING)) {
		if (packet_head == NULL) {
			assert(atem_session_connected(session_index) == true);
		}
		else {
			assert(atem_session_connected(session_index) == false);
		}
		return;
	}
	assert(session_index >= atem_server.sessions_connected);
	assert(atem_session_connected(session_index) == false);
	assert((session->session_id_high << 8 | session->session_id_low) == session->session_id);

	// Disassociates session from previously sent closing request
	assert(session->packet_tail == packet_head);
	assert(packet_head->flags == ATEM_PACKET_FLAG_CLOSING);
	uint16_t packet_session_index_head = session->packet_session_index_head;
	struct atem_packet_session* packet_session = atem_packet_session_get(packet_head, packet_session_index_head);
	assert(packet_session->packet_next == NULL);
	assert(packet_session->session_id == session->session_id);
	assert(packet_session->remote_id_high == 0);
	assert(packet_session->remote_id_low == 0);
	atem_packet_disassociate(packet_head, session->packet_session_index_head);

	DEBUG_PRINTF("Closed session 0x%04x\n", session->session_id);

	// Removes session from server
	atem_session_terminate(session_index);
}



// Acknowledges packets up to ack_id
void atem_session_acknowledge(int16_t session_index, uint16_t ack_id) {
	assert(session_index >= 0);
	assert(session_index < atem_server.sessions_len);

	// Ignores packets to sessions that has started closing
	if (session_index >= atem_server.sessions_connected) {
		return;
	}

	// Gets session to acknowledge packet for
	struct atem_session* session = atem_session_get(session_index);
	assert(session != NULL);
	assert(atem_session_lookup_get(session->session_id) == session_index);

	// Acknowledges all packets up to acknowledge id
	struct atem_packet* packet;
	while ((packet = session->packet_head) != NULL) {
		// Gets remote id for sessions next packet
		assert(!(packet->flags & ATEM_PACKET_FLAG_CLOSING));
		uint16_t packet_session_index_head = session->packet_session_index_head;
		struct atem_packet_session* packet_session = atem_packet_session_get(packet, packet_session_index_head);
		assert(packet_session->session_id == session->session_id);

		// Rejects packets that are considered behind in the sequence
		uint16_t remote_id = packet_session->remote_id_high << 8 | packet_session->remote_id_low;
		if ((ack_id - remote_id) > (0x7fff / 2)) {
			return;
		}

		// Acknowledges packet
		DEBUG_PRINTF("Acknowledging packet 0x%04x for session 0x%04x\n", remote_id, session->session_id);
		session->packet_head = packet_session->packet_next;
		session->packet_session_index_head = packet_session->packet_session_index_next;
		atem_packet_disassociate(packet, packet_session_index_head);
	}
}



// Broadcasts ATEM buffer to all connected sessions
void atem_session_broadcast(uint8_t* buf, uint8_t flags) {
	struct atem_packet* packet = atem_packet_create(buf, atem_server.sessions_connected);
	assert(packet != NULL);

	for (int16_t session_index = 0; session_index < atem_server.sessions_connected; session_index++) {
		struct atem_session* session = atem_session_get(session_index);
		assert(session != NULL);
		assert(atem_session_lookup_get(session->session_id) == session_index);

		// Assigns next session remote id to packet
		session->remote_id++;
		session->remote_id &= 0x7fff;
		buf[ATEM_INDEX_REMOTEID_HIGH] = session->remote_id >> 8;
		buf[ATEM_INDEX_REMOTEID_LOW] = session->remote_id & 0xff;

		// Sends packet to client
		atem_session_send(session, buf);

		// Enqueues packet at beginning of sessions packet queue if empty
		if (session->packet_head == NULL) {
			session->packet_head = packet;
			session->packet_session_index_head = session_index;
		}
		// Enqueues packet at end of sessions queue if it is not empty
		else {
			int16_t packet_session_index = session->packet_session_index_tail;
			session->packet_tail->sessions[packet_session_index].packet_next = packet;
			session->packet_tail->sessions[packet_session_index].packet_session_index_next = session_index;
		}
		session->packet_tail = packet;
		session->packet_session_index_tail = session_index;

		// Initializes packet session for session related data located in packet queue
		struct atem_packet_session* packet_session = &packet->sessions[session_index];
		packet_session->packet_next = NULL;
		packet_session->session_id = session->session_id;
		packet_session->packet_session_index = session_index;
		packet_session->remote_id_high = session->remote_id >> 8;
		packet_session->remote_id_low = session->remote_id & 0xff;
	}

	atem_packet_enqueue(packet, flags);
}
