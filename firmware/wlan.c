#include <stdint.h> // int8_t

#include "./wlan.h" // WLAN_STATION_NOT_CONNECTED
#include "./debug.h" // DEBUG_ERR_PRINTF

#ifdef ESP8266
#include <user_interface.h> // wifi_set_opmode_current, STATION_MODE, wifi_station_get_connect_status, STATION_GOT_IP, wifi_station_get_rssi

// Disables configuration network
void wlan_softap_disable(void) {
	if (!wifi_set_opmode_current(STATION_MODE)) {
		DEBUG_ERR_PRINTF("Failed to disable soft AP\n");
	}
}

// Gets WLAN station RSSI for HTML
int8_t wlan_station_rssi(void) {
	if (wifi_station_get_connect_status() == STATION_GOT_IP) {
		return wifi_station_get_rssi();
	}
	else {
		return WLAN_STATION_NOT_CONNECTED;
	}
}
#endif // ESP8266
