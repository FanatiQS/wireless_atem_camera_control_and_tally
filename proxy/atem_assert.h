// Include guard
#ifndef ATEM_ASSERT_H
#define ATEM_ASSERT_H

#include <stdint.h> // int16_t 

#include "./atem_packet.h" // struct atem_packet

// Does not assert server data structures in release build
#ifndef NDEBUG

void atem_assert_packet(struct atem_packet* packet);
void atem_assert_packets(void);
void atem_assert_session_connected(int16_t session_index);
void atem_assert_session_unconnected(int16_t session_index);
void atem_assert_sessions(void);
void atem_assert(void);

#else // NDEBUG

static inline void atem_assert(void) {
	// Empty in release
}

#endif // NDEBUG

#endif // ATEM_ASSERT_H
