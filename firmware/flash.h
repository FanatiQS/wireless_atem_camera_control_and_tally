// Include guard
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h> // uint8_t, uint32_t

// Masks for configuration flags
#define CONF_FLAG_STATICIP 0x01

// Persistent storage structure for configuration
struct config_t {
	uint32_t localAddr;
	uint32_t netmask;
	uint32_t gateway;
	uint32_t atemAddr;
	uint8_t flags;
	uint8_t dest;
};


#endif // FLASH_H
