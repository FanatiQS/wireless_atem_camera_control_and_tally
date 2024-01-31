#include "./wlan.h"

#ifdef ESP8266
#include <user_interface.h> // wifi_set_opmode_current, STATION_MODE, wifi_station_get_connect_status, STATION_GOT_IP, wifi_station_get_rssi

// Disables configuration network
bool wlan_softap_disable(void) {
	return wifi_set_opmode_current(STATION_MODE);
}

// Gets WLAN station RSSI for HTML
int8_t wlan_station_rssi(void) {
	return (wifi_station_get_connect_status() == STATION_GOT_IP) ? wifi_station_get_rssi() : 1;
}
#endif // ESP8266
