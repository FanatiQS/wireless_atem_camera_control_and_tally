// Include guard
#ifndef FLASH_H
#define FLASH_H

#include <stdint.h> // uint8_t, uint32_t
#include <stdbool.h> // bool

#if defined(ESP8266) /* ESP8266 NONOS SDK */
#include <lwip/ip_addr.h> // required by user_interface for lwip2 link layer to not warn about struct ip_info
#include <user_interface.h> // struct station_config, struct softap_config

// Cached wifi station and softap configurations for HTTP configuration
struct flash_cache_wlan {
	struct station_config station;
	struct softap_config softap;
};

// Linkers to ESP8266 specific wlan data structure in cache
#define CACHE_SSID wlan.station.ssid
#define CACHE_PSK wlan.station.password
#define CACHE_NAME wlan.softap.ssid

#elif defined(ESP_PLATFORM) /* ESP-IDF */
#include <esp_wifi.h> // wifi_config_t

struct flash_cache_wlan {
	wifi_config_t station;
	wifi_config_t softap;
};

// Linkers to ESP8266 specific wlan data structure in cache
#define CACHE_SSID wlan.station.sta.ssid
#define CACHE_PSK wlan.station.sta.password
#define CACHE_NAME wlan.softap.ap.ssid

#endif



// Masks for configuration flags
#define CONF_FLAG_DHCP 0x01

// Persistent storage structure for configuration
struct flash_config {
	uint32_t localip;
	uint32_t netmask;
	uint32_t gateway;
	uint32_t atem_addr;
	uint8_t flags;
	uint8_t dest;
};

// Cached data for HTTP configuration
struct flash_cache {
	struct flash_config config;
	struct flash_cache_wlan wlan;
};

bool flash_config_read(struct flash_config* conf);
bool flash_cache_read(struct flash_cache* cache);
void flash_cache_write(struct flash_cache* cache);

#endif // FLASH_H
