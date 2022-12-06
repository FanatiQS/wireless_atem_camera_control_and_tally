// Include guard
#ifndef DEBUG_H
#define DEBUG_H

#include <lwip/arch.h> // LWIP_PLATFORM_DIAG

#include "./user_config.h" // DEBUG, DEBUG_TALLY, DEBUG_CC

// Throws compilation error if tally or camera control debugging is enabled without general debugging being enabled
#if !DEBUG
#if DEBUG_TALLY
#error Enabling tally debugging requires general debugging to be enabled
#endif // DEBUG_TALLY
#if DEBUG_CC
#error Enabling camera control debugging requires general debugging to be enabled
#endif // DEBUG_CC
#endif // !DEBUG

// Only prints debug info when it is enabled in user_config.h
#if DEBUG
#define DEBUG_PRINTF(...) LWIP_PLATFORM_DIAG((__VA_ARGS__))
#else // DEBUG
#define DEBUG_PRINTF(...)
#endif // DEBUG

#endif // DEBUG_H
