#include <stdint.h> // int8_t

#include "./wlan.h" // WLAN_STATION_NOT_CONNECTED
#include "./debug.h" // DEBUG_ERR_PRINTF

#ifdef ESP8266 /* ESP8266 NONOS SDK */
#include <user_interface.h> // wifi_set_opmode_current, STATION_MODE, wifi_station_get_connect_status, STATION_GOT_IP, wifi_station_get_rssi

// Disables configuration network
void wlan_softap_disable(void) {
	if (!wifi_set_opmode_current(STATION_MODE)) {
		DEBUG_ERR_PRINTF("Failed to disable soft AP\n");
	}
}

// Gets WLAN station RSSI or not connected state for HTML
int8_t wlan_station_rssi(void) {
	if (wifi_station_get_connect_status() == STATION_GOT_IP) {
		return wifi_station_get_rssi();
	}
	else {
		return WLAN_STATION_NOT_CONNECTED;
	}
}

#elif defined(ESP_PLATFORM) /* ESP-IDF */
#include <esp_wifi.h> // esp_wifi_set_mode, WIFI_MODE_STA, esp_wifi_sta_get_ap_info
#include <esp_wifi_types.h> // wifi_ap_record_t
#include <esp_err.h> // esp_err_t, ESP_OK, ESP_ERR_WIFI_NOT_CONNECT

// Disables configuration network
void wlan_softap_disable(void) {
	esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to disable soft AP: %x\n", err);
	}
}

// Gets WLAN station RSSI or not connected state for HTML
int8_t wlan_station_rssi(void) {
	// Gets connected wlan station info
	wifi_ap_record_t ap_info;
	esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

	// Returns RSSI of connected access point
	if (err == ESP_OK) {
		return ap_info.rssi;
	}

	if (err != ESP_ERR_WIFI_NOT_CONNECT) {
		DEBUG_ERR_PRINTF("Failed to get wlan station info: %x\n", err);
	}

	// Station is not connected
	return WLAN_STATION_NOT_CONNECTED;
}

#endif
