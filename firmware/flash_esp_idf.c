#include <stdbool.h> // bool, true, false
#include <string.h> // memcmp

#include "./flash.h" // struct config_t

#include <esp_spi_flash.h> // spi_flash_read, SPI_FLASH_SEC_SIZE, spi_flash_erase_sector, spi_flash_write
#include <esp_err.h> // esp_err_t, ESP_OK
#include <esp_wifi.h> // esp_wifi_get_config, ESP_IF_WIFI_STA, ESP_IF_WIFI_AP, esp_wifi_set_config
#include <esp_system.h> // esp_restart

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_HTTP_PRINTF
#include "./flash.h" // struct config_t, struct cache_t

// Configuration is located in the first flash sector after second OTA partition
#define CONFIG_START (0x290000)

// Reads configuration from persistent storage
bool flash_config_read(struct config_t* conf) {
	esp_err_t err = spi_flash_read(CONFIG_START, conf, sizeof(*conf));
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read config data from flash: %x\n", err);
		return false;
	}
	return true;
}

// Reads configuration for HTTP from persistent storage
bool flash_cache_read(struct cache_t* cache) {
	esp_err_t err;

	// Reads wlan configuration
	err = esp_wifi_get_config(ESP_IF_WIFI_STA, &cache->wlan.station);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read station config: %x\n", err);
		return false;
	}

	// Reads softap configuration for device name
	err = esp_wifi_get_config(ESP_IF_WIFI_AP, &cache->wlan.softap);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read softap config: %x\n", err);
		return false;
	}

	// Reads waccat configuration
	return flash_config_read(&cache->config);
}

// Writes cached HTTP configuration to persistent storage and reboots
void flash_cache_write(struct cache_t* cache) {
	// Writes wlan configuration and device name to persistent storage
	if (esp_wifi_set_config(ESP_IF_WIFI_STA, &cache->wlan.station) != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to write station config");
		return;
	}
	if (esp_wifi_set_config(ESP_IF_WIFI_AP, &cache->wlan.softap) != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to write softap config");
		return;
	}

	// Writes configuration to persistent storage if changed
	struct config_t conf_current;
	if (!flash_config_read(&conf_current)) {
		return;
	}
	if (memcmp(&conf_current, &cache->config, sizeof(conf_current)) != 0) {
		if (spi_flash_erase_sector(CONFIG_START / SPI_FLASH_SEC_SIZE) != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to erase flash sector\n");
			return;
		}
		if (spi_flash_write(CONFIG_START, &cache->config, sizeof(cache->config)) != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to write config data\n");
			return;
		}
	}

	// Restarts device after flash write
	DEBUG_HTTP_PRINTF("Rebooting...\n");
	esp_restart();
}
