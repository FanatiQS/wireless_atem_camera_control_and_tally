// Include guard
#ifndef ATEM_CACHE_H
#define ATEM_CACHE_H

#include <stdint.h> // uint8_t, uint16_t, uint32_t

#include "./atem_session.h" // struct atem_session

void atem_cache_init(uint8_t source_count);
void atem_cache_release(void);
void atem_cache_dump(struct atem_session* session);
void atem_cache_update(uint8_t* buf, uint16_t len);

#endif // ATEM_CACHE_H
