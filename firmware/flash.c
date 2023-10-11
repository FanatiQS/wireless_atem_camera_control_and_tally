#include <stdbool.h> // bool true, false
#include <string.h> // memcmp

#include "./debug.h" // DEBUG_ERR_PRINTF
#include "./flash.h" // struct config_t, struct cache_t



#ifdef ESP8266
#include <user_interface.h> // wifi_station_get_config, wifi_softap_get_config, wifi_station_set_config, wifi_softap_set_config
#include <spi_flash.h> // spi_flash_erase_sector, spi_flash_write, spi_flash_read, SPI_FLASH_RESULT_OK, spi_flash_get_id, SPI_FLASH_SEC_SIZE

// Sets configuration address to last memory page of flash before system data, same as arduino uses for EEPROM
#define FLASH_SIZE (1 << ((spi_flash_get_id() >> 16) & 0xff))
#define CONFIG_START (FLASH_SIZE - (12 + 4 + 4) * 1024)
#endif // ESP8266



// Reads configuration from persistent storage
bool flash_config_read(struct config_t* conf) {
#ifdef ESP8266
	if (spi_flash_read(CONFIG_START, (uint32_t*)conf, sizeof(*conf)) != SPI_FLASH_RESULT_OK) {
		DEBUG_ERR_PRINTF("Failed to read device configuration\n");
		return false;
	}
	return true;
#endif // ESP8266
}

// Writes configuration to persistent storage
static inline bool flash_config_write(struct config_t* conf) {
#ifdef ESP8266
	if (spi_flash_erase_sector((uint16_t)(CONFIG_START / SPI_FLASH_SEC_SIZE))) {
		DEBUG_ERR_PRINTF("Failed to erase flash sector\n");
		return false;
	}
	if (spi_flash_write(CONFIG_START, (uint32_t*)conf, sizeof(*conf)) != SPI_FLASH_RESULT_OK) {
		DEBUG_ERR_PRINTF("Failed to write config data\n");
		return false;
	}
	return true;
#endif // ESP8266
}



// Reads flash configuration and other platform specific configurations for HTTP cache
bool flash_cache_read(struct cache_t* cache) {
#ifdef ESP8266
	if (!wifi_station_get_config(&(cache->wlan.station))) {
		DEBUG_ERR_PRINTF("Failed to read station config\n");
		return false;
	}
	if (!wifi_softap_get_config(&(cache->wlan.softap))) {
		DEBUG_ERR_PRINTF("Failed to read softap config\n");
		return false;
	}
#endif // ESP8266
	return flash_config_read(&(cache->config));
}

// Writes flash configuration and other platform specific configurations for HTTP cache
void flash_cache_write(struct cache_t* cache) {
#ifdef ESP8266
	if (!wifi_station_set_config(&cache->wlan.station)) {
		DEBUG_ERR_PRINTF("Failed to write station config\n");
		return;
	}
	cache->wlan.softap.ssid_len = 0;
	if (!wifi_softap_set_config(&cache->wlan.softap)) {
		DEBUG_ERR_PRINTF("Failed to write softap config\n");
		return;
	}
#endif // ESP8266

	// Only updates flash if required
	struct config_t conf;
	if (!flash_config_read(&conf)) return;
	if (memcmp(&conf, &(cache->config), sizeof(conf))) {
		if (!flash_config_write(&(cache->config))) return;
	}
}
