// Include guard
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h> // uint8_t, uint32_t
#include <stdbool.h> // bool

#ifdef ESP8266
#include <user_interface.h> // struct station_config, struct softap_config

// Cached wifi station and softap configurations for HTTP configuration
struct cache_wlan {
	struct station_config station;
	struct softap_config softap;
};
#define CACHE_WLAN struct cache_wlan

// Linkers to ESP8266 specific wlan data structure in cache
#define CACHE_SSID wlan.station.ssid
#define CACHE_PSK wlan.station.password
#define CACHE_NAME wlan.softap.ssid
#endif // ESP8266

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
#ifndef CACHE_SSID
	char ssid[32];
#define CACHE_SSID config.ssid
#endif // CACHE_SSID
#ifndef CACHE_PSK
	char psk[64];
#define CACHE_PSK config.psk
#endif // CACHE_PSK
#ifndef CACHE_NAME
	char name[32];
#define CACHE_NAME config.name
#endif // CACHE_NAME
};

// Cached data for HTTP configuration
struct cache_t {
	struct config_t config;
#ifdef CACHE_WLAN
	CACHE_WLAN wlan;
#endif // CONFIG_WLAN
};

bool flash_config_read(struct config_t* conf);
bool flash_cache_read(struct cache_t* cache);
bool flash_cache_write(struct cache_t* cache);

#endif // FLASH_H
