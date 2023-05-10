// Include guard
#ifndef UDP_H
#define UDP_H

#include <stdint.h> // uint32_t, uint8_t

#include <lwip/udp.h> // struct udp_pcb

// String representing the current ATEM connection status
extern const char* atem_state;

// Initializes ATEM UDP client
struct udp_pcb* atem_init(uint32_t, uint8_t);

#endif // UDP_H
