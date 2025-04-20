// Include guard
#ifndef ATEM_SESSION_H
#define ATEM_SESSION_H

#include <stdint.h> // uint8_t, uint16_t, int16_t
#include <stdbool.h> // bool

#include <netinet/in.h> // struct sockaddr_in

#include "./atem_packet.h" // struct atem_packet

// ATEM session containing information about the client connection
struct atem_session {
	// Sessions local packet queue
	struct atem_packet* packet_head;
	struct atem_packet* packet_tail;
	uint16_t packet_session_index_head;
	uint16_t packet_session_index_tail;
	// The remote id is used when transmitting a packet that requires an acknowledgement
	uint16_t remote_id;
	// The session id of the session to use when iterating through all sessions instead of starting with session id
	uint16_t session_id;
	// Can either be server assigned session id or client assigned session id and is used when sending packets	
	uint8_t session_id_high;
	uint8_t session_id_low;
	// The ip address and port of the remote peer (client) to send packets to
	struct sockaddr_in peer_addr;
};

int16_t atem_session_lookup_get(uint16_t session_id);
void atem_session_lookup_clear(uint16_t session_id);
struct atem_session* atem_session_get(int16_t session_index);
bool atem_session_peer_validate(struct atem_session* session, struct sockaddr_in* peer_addr);
void atem_session_send(struct atem_session* session, uint8_t* buf);

void atem_session_create(uint8_t session_id_high, uint8_t session_id_low, struct sockaddr_in* peer_addr);
void atem_session_connect(uint8_t session_id_high, uint8_t session_id_low, struct sockaddr_in* peer_addr);
void atem_session_drop(int16_t session_index);
void atem_session_terminate(int16_t session_index);
void atem_session_closing(int16_t session_index);
void atem_session_closed(int16_t session_index);

void atem_session_acknowledge(int16_t session_index, uint16_t ack_id);
void atem_session_packet_push(struct atem_session* session, struct atem_packet* packet, uint16_t packet_session_index);

#endif // ATEM_SESSION_H
