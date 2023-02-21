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
#if !DEBUG
#define DEBUG_PRINTF(...)
#elif !defined(DEBUG_PRINTF) // DEBUG
#define DEBUG_PRINTF(...) LWIP_PLATFORM_DIAG((__VA_ARGS__))
#endif // DEBUG

// Prints integer IP address
#define _IP_VALUE(ip, shift) (((ip) >> shift) & 0xff)
#define IP_VALUE(ip) _IP_VALUE(ip, 0), _IP_VALUE(ip, 8), _IP_VALUE(ip, 16), _IP_VALUE(ip, 24)
#define IP_FMT "%d.%d.%d.%d"
#define DEBUG_IP(label, local, netmask, gw)\
	DEBUG_PRINTF(\
		label ":\n"\
		"\tLocal Address: " IP_FMT "\n"\
		"\tSubnet Mask: " IP_FMT "\n"\
		"\tGateway: " IP_FMT "\n",\
		IP_VALUE(local),\
		IP_VALUE(netmask),\
		IP_VALUE(gw)\
	)

// Wraps argument into a string
#define _WRAP(arg) #arg
#define WRAP(arg) _WRAP(arg)

#endif // DEBUG_H
