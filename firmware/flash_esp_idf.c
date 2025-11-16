#include <stdbool.h> // bool, true, false
#include <string.h> // memcmp
#include <assert.h> // _Static_assert

#include "./flash.h" // struct flash_config

#include <esp_flash.h> // esp_flash_read, esp_flash_erase_region, esp_flash_write
#include <esp_err.h> // esp_err_t, ESP_OK
#include <esp_wifi.h> // esp_wifi_get_config, WIFI_IF_STA, WIFI_IF_AP, esp_wifi_set_config
#include <esp_system.h> // esp_restart
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <spi_flash_mmap.h> // SPI_FLASH_SEC_SIZE
#else
#include <esp_spi_flash.h> // SPI_FLASH_SEC_SIZE
#endif // ESP_IDF_VERSION

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_HTTP_PRINTF
#include "./flash.h" // struct flash_config, struct flash_cache

// Configuration is located in the first flash sector after second OTA partition
#define CONFIG_START (0x290000)

// Asserts union pointer addresses
_Static_assert(
	offsetof(struct flash_cache_wlan, name) == offsetof(struct flash_cache_wlan, softap.ap.ssid),
	"Config union pointer mismatch on name"
);
_Static_assert(
	offsetof(struct flash_cache_wlan, ssid) == offsetof(struct flash_cache_wlan, station.sta.ssid),
	"Config union pointer mismatch on ssid"
);
_Static_assert(
	offsetof(struct flash_cache_wlan, psk) == offsetof(struct flash_cache_wlan, station.sta.password),
	"Config union pointer mismatch on psk"
);

// Reads configuration from persistent storage
bool flash_config_read(struct flash_config* conf) {
	esp_err_t err = esp_flash_read(NULL, conf, CONFIG_START, sizeof(*conf));
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read config data from flash: %x\n", err);
		return false;
	}
	return true;
}

// Reads configuration for HTTP from persistent storage
bool flash_cache_read(struct flash_cache* cache) {
	esp_err_t err;

	// Reads wlan configuration
	err = esp_wifi_get_config(WIFI_IF_STA, &cache->wlan.station);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read station config: %x\n", err);
		return false;
	}

	// Reads softap configuration for device name
	err = esp_wifi_get_config(WIFI_IF_AP, &cache->wlan.softap);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read softap config: %x\n", err);
		return false;
	}

	// Reads waccat configuration
	return flash_config_read(&cache->config);
}

// Writes cached HTTP configuration to persistent storage and reboots
void flash_cache_write(struct flash_cache* cache) {
	esp_err_t err;

	// Writes wlan configuration and device name to persistent storage
	err = esp_wifi_set_config(WIFI_IF_STA, &cache->wlan.station);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to write station config: %x\n", err);
		return;
	}
	err = esp_wifi_set_config(WIFI_IF_AP, &cache->wlan.softap);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to write softap config: %x\n", err);
		return;
	}

	// Writes configuration to persistent storage if changed
	struct flash_config conf_current;
	if (!flash_config_read(&conf_current)) {
		return;
	}
	if (memcmp(&conf_current, &cache->config, sizeof(conf_current)) != 0) {
		err = esp_flash_erase_region(NULL, CONFIG_START, SPI_FLASH_SEC_SIZE);
		if (err != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to erase flash sector: %x\n", err);
			return;
		}
		err = esp_flash_write(NULL, &cache->config, CONFIG_START, sizeof(cache->config));
		if (err != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to write config data: %x\n", err);
			return;
		}
	}

	// Restarts device after flash write
	DEBUG_HTTP_PRINTF("Rebooting...\n");
	esp_restart();
}
