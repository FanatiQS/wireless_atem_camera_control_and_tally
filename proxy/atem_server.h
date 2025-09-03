// Include guard
#ifndef ATEM_SERVER_H
#define ATEM_SERVER_H

#include <stdint.h> // uint8_t, uint16_t, int16_t, UINT16_MAX
#include <time.h> // struct timespec
#include <stdbool.h> // bool

#include "./atem_packet.h" // struct atem_packet
#include "./atem_session.h" // struct atem_session

// How much to grow the sessions array by when it runs out of slots
#define ATEM_SERVER_SESSIONS_MULTIPLIER (1.6f)

// ATEM server containing information about sessions and in transit packets
struct atem_server {
	// Global packet queue for retransmits
	struct atem_packet* packet_queue_head;
	struct atem_packet* packet_queue_tail;
	// Dynamic array of all available session contexts
	struct atem_session* sessions;
	// ATEM server UDP socket
	int sock;
	// Number of slots available in the sessions array
	uint16_t sessions_size;
	// Number of available sessions in the sessions array
	uint16_t sessions_len;
	// Number of connected sessions at the head of the sessions array
	uint16_t sessions_connected;
	// Configurable max number of sessions allowed
	uint16_t sessions_limit;
	// Last session id assigned
	uint16_t session_id_last;
	// Configurable number of milliseconds without acknowledgement before retransmitting packet
	uint16_t retransmit_delay;
	// Configurable number of milliseconds between pings
	uint16_t ping_interval;
	// Timestamp from where next ping timeout is calculated from
	struct timespec ping_timestamp;
	// Indicates if the server has started closing
	bool closing;
	// Lookup table for translating session id to sessions array index
	int16_t session_lookup_table[UINT16_MAX + 1];
};

extern struct atem_server atem_server;

bool atem_server_init(void);
void atem_server_recv(void);
void atem_server_broadcast(struct atem_packet* packet, uint8_t flags);

void atem_server_flush(void);
void atem_server_close(void);
bool atem_server_closed(void);
void atem_server_restart(void);
void atem_server_release(void);

#endif // ATEM_SERVER_H
