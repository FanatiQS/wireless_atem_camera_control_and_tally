// Include guard
#ifndef ATEM_PACKET_H
#define ATEM_PACKET_H

#include <stdint.h> // uint8_t, uint16_t
#include <time.h> // struct timespec

// ATEM packet flags
enum {
	// Packet should free its buffer when releasing the packet (it is heap allocated)
	ATEM_PACKET_FLAG_RELEASE = 1,
	// Packet is performing a closing handshake
	ATEM_PACKET_FLAG_CLOSING = 2
};

// Session information for the packet it is embedded into
struct atem_packet_session {
	// The sessions next packet
	struct atem_packet* packet_next;
	// The index of the sessions next packet
	uint16_t packet_session_index_next;
	// The session id of the session packet_session belongs to
	uint16_t session_id;
	// When retransmitting the packet, this index is used to reference packet_sessions outside the bounds of sessions_remaining
	// The index either points to itself if its session should get a retransmit and it is located within the bounds of sessions_remaining, point to a packet_session outside the sessions_remaining bounds that should get retransmit or points to the packet_session within the bounds if it should get retransmit but is outside the bounds of sessions_remaining
	uint16_t packet_session_index;
	// Remote id to use when retransmitting
	uint8_t remote_id_high;
	uint8_t remote_id_low;
};

// ATEM packet that can be referenced both through the global packet queue and from sessions local packet queue
struct atem_packet {
	// Global packet queue sorted based on how close its timeout is
	struct atem_packet* next;
	struct atem_packet* prev;
	// The actual packet data to transmit
	uint8_t* buf;
	// Number of sessions that still haven't acknowledged the packet, used for retransmits
	uint16_t sessions_remaining;
	// Length of the sessions array, should only used for asserts and debug printing
	#ifndef NDEBUG
	uint16_t sessions_len;
	#endif // NDEBUG
	// Number of times the packet has left to retransmit its content to remaining sessions before terminating them
	uint8_t resends_remaining;
	// Flags for packet, refer to enum for more details
	uint8_t flags;
	// Timestamp for when this packet should be retransmitted
	struct timespec timeout;
	// Flexible array for all sessions connected to this packet
	struct atem_packet_session sessions[];
};

struct atem_packet_session* atem_packet_session_get(struct atem_packet* packet, uint16_t packet_session_index);
struct atem_packet* atem_packet_create(uint8_t* buf, uint16_t sessions_count);
void atem_packet_enqueue(struct atem_packet* packet, uint8_t flags);
void atem_packet_flush(struct atem_packet* packet, uint16_t packet_session_index);
void atem_packet_release(struct atem_packet* packet);
void atem_packet_disassociate(struct atem_packet* packet, uint16_t packet_session_index);
void atem_packet_retransmit(void);
void atem_packet_ping(void);

#endif // ATEM_PACKET_H
