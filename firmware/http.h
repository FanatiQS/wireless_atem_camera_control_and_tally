// Include guard
#ifndef HTTP_H
#define HTTP_H

#include <stdint.h> // uint32_t, uint8_t

// Mask for configuration flags
#define CONF_FLAG_STATICIP 1

// Persistent storage structure
struct config_t {
	uint32_t localAddr;
	uint32_t netmask;
	uint32_t gateway;
	uint32_t atemAddr;
	uint8_t flags;
	uint8_t dest;
};

// Initializes HTTP configuration server
bool http_init(void);

#endif // HTTP_H
