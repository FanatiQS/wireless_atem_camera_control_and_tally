// Include guard
#ifndef ATEM_SOCK_H
#define ATEM_SOCK_H

#include <stdint.h> // uint32_t, uint8_t

#include <lwip/udp.h> // struct udp_pcb

// String representing the current ATEM connection status
extern const char* atem_state;

// Initializes ATEM UDP client
struct udp_pcb* atem_init(uint32_t addr, uint8_t dest);

#endif // ATEM_SOCK_H
