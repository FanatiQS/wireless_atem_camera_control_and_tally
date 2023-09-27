// Include guard
#ifndef DEBUG_H
#define DEBUG_H

#include <lwip/arch.h> // LWIP_PLATFORM_DIAG

#include "./user_config.h" // DEBUG, DEBUG_TALLY, DEBUG_CC, DEBUG_ATEM, DEBUG_HTTP



// Throws compilation error if specific debugging is enabled without general debugging being enabled
#if !DEBUG
#if DEBUG_TALLY
#error Enabling tally debugging requires general debugging to be enabled
#endif // DEBUG_TALLY
#if DEBUG_CC
#error Enabling camera control debugging requires general debugging to be enabled
#endif // DEBUG_CC
#if DEBUG_ATEM
#error Enabling ATEM protocol debugging requires general debugging to be enabled
#endif // DEBUG_ATEM
#if DEBUG_HTTP
#error Enabling HTTP debugging requires general debugging to be enabled
#endif // DEBUG_HTTP
#endif // !DEBUG



// Only prints debug info when it is enabled in user_config.h
#if !DEBUG
#define DEBUG_PRINTF(...)
#elif !defined(DEBUG_PRINTF) // DEBUG
#define DEBUG_PRINTF(...) LWIP_PLATFORM_DIAG((__VA_ARGS__))
#endif // DEBUG

// Prints integer IP address
#define IP_OCTET(ip, shift) (uint8_t)(((ip) >> shift) & 0xff)
#define IP_VALUE(ip) IP_OCTET(ip, 0), IP_OCTET(ip, 8), IP_OCTET(ip, 16), IP_OCTET(ip, 24)
#define IP_FMT "%u.%u.%u.%u"
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



// Prints error messages with prefix
#define DEBUG_ERR_PRINTF(...) DEBUG_PRINTF("[ ERR ] " __VA_ARGS__)

// Only prints HTTP debug info when it is enabled in user_config.h
#if DEBUG_HTTP
#define DEBUG_HTTP_PRINTF(...) DEBUG_PRINTF("[ HTTP ] " __VA_ARGS__)
#else // DEBUG_HTTP
#define DEBUG_HTTP_PRINTF(...)
#endif // DEBUG_HTTP



// Wraps argument into a string
#define WRAP_INNER(arg) #arg
#define WRAP(arg) WRAP_INNER(arg)

#endif // DEBUG_H
