#include <stddef.h> // NULL

#include "./atem_session.h" // struct atem_session
#include "./atem_packet.h" // struct atem_packet
#include "./atem_cache.h"

// @todo
void atem_cache_dump(struct atem_session* session, struct atem_packet* packet) {
	atem_packet_release(packet);
	session->packet_head = NULL;
	session->packet_tail = NULL;
}
