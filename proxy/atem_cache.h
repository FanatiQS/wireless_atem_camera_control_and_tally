// Include guard
#ifndef ATEM_CACHE_H
#define ATEM_CACHE_H

#include "./atem_session.h" // struct atem_session
#include "./atem_packet.h" // struct atem_packet

void atem_cache_dump(struct atem_session* session, struct atem_packet* packet);

#endif // ATEM_CACHE_H
