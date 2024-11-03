// Include guard
#ifndef ATEM_SOCK_H
#define ATEM_SOCK_H

#include <stdint.h> // uint8_t, uint32_t

#include <lwip/udp.h> // struct udp_pcb
#include <lwip/netif.h> // struct netif
#include <lwip/ip_addr.h> // ip_addr_t

#include "../core/atem.h" // struct atem

// String representing the current ATEM connection status
extern const char* atem_state;

// All possible connection states to the ATEM switcher except unconnected
extern const char* const atem_state_connected;
extern const char* const atem_state_dropped;
extern const char* const atem_state_rejected;
extern const char* const atem_state_disconnected;

// The connection context to the ATEM switcher
extern struct atem atem;

// Initializes ATEM UDP client
struct udp_pcb* atem_init(uint32_t addr, uint8_t dest);

#endif // ATEM_SOCK_H
