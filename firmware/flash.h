// Include guard
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h> // uint8_t, uint32_t

#ifdef ESP8266
#include <user_interface.h> // struct station_config, struct softap_config

// Linkers to ESP8266 specific wlan data structure in cache
#define CACHE_SSID wlan_station.ssid
#define CACHE_PSK wlan_station.password
#define CACHE_NAME wlan_softap.ssid
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

// Cached data for HTTP client during HTTP POSt configuration
struct cache_t {
	struct config_t config;
#ifdef ESP8266
	struct station_config wlan_station;
	struct softap_config wlan_softap;
#endif // ESP8266
};

bool flash_config_read(struct config_t* conf);
bool flash_cache_read(struct cache_t* cache);
bool flash_cache_write(struct cache_t* cache);

#endif // FLASH_H
