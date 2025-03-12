// Include guard
#ifndef ATEM_DEBUG_H
#define ATEM_DEBUG_H

#include <stdio.h> // fprintf, stderr
#include <stdint.h> // uint8_t, uint16_t

#include "./atem_packet.h" // struct atem_packet
#include "./atem_session.h" // struct atem_session

#if DEBUG

#define DEBUG_PRINTF(...) fprintf(stderr, "[ DEBUG ] " __VA_ARGS__)
#define DEBUG_PRINT_BUF(buf, len) atem_debug_print_buf(buf, len)

void atem_debug_print_buf(const uint8_t* buf, uint16_t len);

// These functions are meant to be used in a debugger or temporarily in code
void atem_debug_print_packet(struct atem_packet* packet);
void atem_debug_print_packets(void);
void atem_debug_print_session(struct atem_session* session);
void atem_debug_print_sessions(void);
void atem_debug_print_table(void);
void atem_debug_print_server(void);
void atem_debug_print(void);

#else // DEBUG

#define DEBUG_PRINTF(...)
#define DEBUG_PRINT_BUF(buf, len)

#endif // DEBUG

#endif // ATEM_DEBUG_H
