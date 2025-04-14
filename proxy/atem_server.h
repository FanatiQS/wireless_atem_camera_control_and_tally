// Include guard
#ifndef ATEM_SERVER_H
#define ATEM_SERVER_H

#include <stdint.h> // uint16_t, int16_t, UINT16_MAX
#include <time.h> // struct timespec
#include <stdbool.h> // bool

#include "./atem_packet.h" // struct atem_packet_t
#include "./atem_session.h" // struct atem_session_t

// How much to grow the sessions array by when it runs out of slots
#define ATEM_SERVER_SESSIONS_MULTIPLIER (1.6f)

struct atem_server {
	struct atem_packet* packet_queue_head;
	struct atem_packet* packet_queue_tail;
	struct atem_session* sessions;
	int sock;
	uint16_t sessions_size;
	uint16_t sessions_len;
	uint16_t sessions_connected;
	uint16_t sessions_limit;
	uint16_t session_id_last;
	uint16_t retransmit_delay;
	uint16_t ping_interval;
	struct timespec ping_timeout;
	bool closing;
	int16_t session_lookup_table[UINT16_MAX + 1];
};

extern struct atem_server atem_server;

bool atem_server_init(void);
void atem_server_recv(void);
void atem_server_close(void);
bool atem_server_closed(void);
void atem_server_release(void);

#endif // ATEM_SERVER_H
