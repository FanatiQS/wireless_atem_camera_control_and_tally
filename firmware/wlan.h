// Include guard
#ifndef WLAN_H
#define WLAN_H

#include <stdbool.h> // bool, true, false
#include <stdint.h> // int8_t

#include "./flash.h" // struct flash_config, CONF_FLAG_DHCP
#include "./debug.h" // DEBUG_IP, DEBUG_PRINTF

#define WLAN_STATION_NOT_CONNECTED 1

void wlan_softap_disable(void);
int8_t wlan_station_rssi(void);

// Gets WLAN station DHCP client status from persistent storage data
static inline bool wlan_station_dhcp_get(struct flash_config* conf) {
	if (conf->flags & CONF_FLAG_DHCP) {
		DEBUG_PRINTF("Using DHCP\n");
		return true;
	}
	else {
		DEBUG_IP("Using static IP", conf->localip, conf->netmask, conf->gateway);
		return false;
	}
}

#endif // WLAN_H
