// Include guard
#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h> // printf

#include <lwip/init.h> // LWIP_VERSION_STRING

#include "./user_config.h" // DEBUG, DEBUG_TALLY, DEBUG_CC, DEBUG_ATEM, DEBUG_HTTP, DEBUG_DNS
#include "./init.h" // FIRMWARE_VERSION_STRING



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
#if DEBUG_DNS
#error Enabling DNS debugging requires general debugging to be enabled
#endif // DEBUG_DNS
#endif // !DEBUG

// Defines debug flag labels
#if DEBUG_TALLY
#define BOOT_INFO_DEBUG_TALLY "Tally debug: enabled\n"
#else // DEBUG_TALLY
#define BOOT_INFO_DEBUG_TALLY "Tally debug: disabled\n"
#endif // DEBUG_TALLY
#if DEBUG_CC
#define BOOT_INFO_DEBUG_CC "Camera control debug: enabled\n"
#else // DEBUG_CC
#define BOOT_INFO_DEBUG_CC "Camera control debug: disabled\n"
#endif // DEBUG_CC
#if DEBUG_ATEM
#define BOOT_INFO_DEBUG_ATEM "ATEM debug: enabled\n"
#else // DEBUG_ATEM
#define BOOT_INFO_DEBUG_ATEM "ATEM debug: disabled\n"
#endif // DEBUG_ATEM
#if DEBUG_HTTP
#define BOOT_INFO_DEBUG_HTTP "HTTP debug: enabled\n"
#else // DEBUG_HTTP
#define BOOT_INFO_DEBUG_HTTP "HTTP debug: disabled\n"
#endif // DEBUG_HTTP
#if DEBUG_DNS
#define BOOT_INFO_DEBUG_DNS "DNS debug: enabled\n"
#else // DEBUG_DNS
#define BOOT_INFO_DEBUG_DNS "DNS debug: disabled\n"
#endif // DEBUG_DNS

// Defines undefined version for compilers where the macro does not exist
#ifndef __VERSION__
#define __VERSION__ "undefined"
#endif // __VERSION__

// Static information to print at boot
#define DEBUG_BOOT_INFO\
	"\n\n"\
	"Booting...\n"\
	BOOT_INFO_DEBUG_TALLY\
	BOOT_INFO_DEBUG_CC\
	BOOT_INFO_DEBUG_ATEM\
	BOOT_INFO_DEBUG_HTTP\
	BOOT_INFO_DEBUG_DNS\
	"Using compiler version: " __VERSION__ "\n"\
	"Firmware version: " FIRMWARE_VERSION_STRING "\n"\
	"Using LwIP version: " LWIP_VERSION_STRING "\n"



// Only prints debug info when it is enabled in user_config.h
#if !DEBUG
#define DEBUG_PRINTF(...)
#elif !defined(DEBUG_PRINTF) // DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
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

// Prints wlan SSID and PSK
#define DEBUG_WLAN(label, ssid, psk)\
	DEBUG_PRINTF(\
		label " SSID: \"%.*s\"\n"\
		label " PSK: \"%.*s\"\n",\
		(int)sizeof(ssid), ssid,\
		(int)sizeof(psk), psk\
	)



// Prints error messages with prefix
#define DEBUG_ERR_PRINTF(...) DEBUG_PRINTF("[ ERR ] " __VA_ARGS__)

// Only prints HTTP debug info when it is enabled in user_config.h
#if DEBUG_HTTP
#define DEBUG_HTTP_PRINTF(...) DEBUG_PRINTF("[ HTTP ] " __VA_ARGS__)
#else // DEBUG_HTTP
#define DEBUG_HTTP_PRINTF(...)
#endif // DEBUG_HTTP

// Only prints DNS captive portal debug info when it is enabled in user_config.h
#if DEBUG_DNS
#define DEBUG_DNS_PRINTF(...) DEBUG_PRINTF("[ DNS ] " __VA_ARGS__)
#else // DEBUG_DNS
#define DEBUG_DNS_PRINTF(...)
#endif // DEBUG_DNS

// Only prints Tally debug info when it is enabled in user_config.h
#if DEBUG_TALLY
#define DEBUG_TALLY_PRINTF(...) DEBUG_PRINTF("[ TALLY ] " __VA_ARGS__)
#else // DEBUG_TALLY
#define DEBUG_TALLY_PRINTF(...)
#endif // DEBUG_TALLY

// Only prints ATEM debug info when it is enabled in user_config.h
#if DEBUG_ATEM
#define DEBUG_ATEM_PRINTF(...) DEBUG_PRINTF("[ ATEM ] " __VA_ARGS__)
#else // DEBUG_ATEM
#define DEBUG_ATEM_PRINTF(...)
#endif // DEBUG_ATEM



// Wraps argument into a string
#define WRAP_INNER(arg) #arg
#define WRAP(arg) WRAP_INNER(arg)

#endif // DEBUG_H
