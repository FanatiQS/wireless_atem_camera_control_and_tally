#include <stdbool.h> // bool true, false
#include <string.h> // memcmp
#include <stdint.h> // uint16_t, uint32_t

#include <lwip/ip_addr.h> // required by user_interface for lwip2 link layer to not warn about struct ip_info
#include <user_interface.h> // wifi_station_get_config, wifi_softap_get_config, wifi_station_set_config, wifi_softap_set_config
#include <spi_flash.h> // spi_flash_erase_sector, spi_flash_write, spi_flash_read, SPI_FLASH_RESULT_OK, spi_flash_get_id, SPI_FLASH_SEC_SIZE, wifi_set_event_handler_cb, system_restart

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_HTTP_PRINTF
#include "./flash.h" // struct config_t, struct cache_t

/*
 * Sets configuration address for storing custom configurations.
 * Uses last page before "User Data" in flash (same as arduino uses for EEPROM).
 * User data page is only followed by 3 statically sized areas:
 * 		"RF_CAL Parameter Area" (4KB)
 * 		"Default RF Parameter Area" (4KB),
 * 		"System Parameter Area" (12KB),
 * Flash maps are documented in chapter 4 of https://www.espressif.com/sites/default/files/documentation/2a-esp8266-sdk_getting_started_guide_en.pdf
 */
#define FLASH_SIZE (1 << ((spi_flash_get_id() >> 16) & 0xff))
#define CONFIG_START (FLASH_SIZE - (12 + 4 + 4) * 1024)



// Reads configuration from persistent storage
bool flash_config_read(struct config_t* conf) {
	if (spi_flash_read(CONFIG_START, (uint32_t*)conf, sizeof(*conf)) != SPI_FLASH_RESULT_OK) {
		DEBUG_ERR_PRINTF("Failed to read device configuration\n");
		return false;
	}
	return true;
}

// Reads flash configuration and other platform specific configurations for HTTP cache
bool flash_cache_read(struct cache_t* cache) {
	if (!wifi_station_get_config(&cache->wlan.station)) {
		DEBUG_ERR_PRINTF("Failed to read station config\n");
		return false;
	}
	if (!wifi_softap_get_config(&cache->wlan.softap)) {
		DEBUG_ERR_PRINTF("Failed to read softap config\n");
		return false;
	}
	return flash_config_read(&cache->config);
}

// Writes flash configuration and other platform specific configurations for HTTP cache
void flash_cache_write(struct cache_t* cache) {
	// Enables softap to commit device name
	if (!wifi_set_opmode(STATIONAP_MODE)) {
		DEBUG_ERR_PRINTF("Failed to set opcode\n");
		return;
	}

	// Writes wlan station and softap configuration to flash
	if (!wifi_station_set_config(&cache->wlan.station)) {
		DEBUG_ERR_PRINTF("Failed to write station config\n");
		return;
	}
	cache->wlan.softap.ssid_len = 0;
	if (!wifi_softap_set_config(&cache->wlan.softap)) {
		DEBUG_ERR_PRINTF("Failed to write softap config\n");
		return;
	}

	// Writes configuration to persistent storage if changed
	struct config_t currentConf;
	if (!flash_config_read(&currentConf)) {
		return;
	}
	if (memcmp(&currentConf, &cache->config, sizeof(currentConf)) != 0) {
		if (spi_flash_erase_sector((uint16_t)(CONFIG_START / SPI_FLASH_SEC_SIZE))) {
			DEBUG_ERR_PRINTF("Failed to erase flash sector\n");
			return;
		}
		if (spi_flash_write(CONFIG_START, (uint32_t*)&cache->config, sizeof(cache->config)) != SPI_FLASH_RESULT_OK) {
			DEBUG_ERR_PRINTF("Failed to write config data\n");
			return;
		}
	}

	// Restarts device after flash write
	DEBUG_HTTP_PRINTF("Rebooting...\n");
	wifi_set_event_handler_cb(NULL);
	system_restart();
}
