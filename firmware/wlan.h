// Include guard
#ifndef WLAN_H
#define WLAN_H

#include <stdbool.h> // bool, true, false
#include <stdint.h> // int8_t

#include "./flash.h" // struct config_t, CONF_FLAG_DHCP
#include "./debug.h" // DEBUG_IP, DEBUG_PRINTF

bool wlan_softap_disable(void);
int8_t wlan_station_rssi(void);

// Gets WLAN station DHCP client status from persistent storage data
static inline bool wlan_station_dhcp_get(struct config_t conf) {
	if (conf.flags & CONF_FLAG_DHCP) {
		DEBUG_PRINTF("Using DHCP\n");
		return true;
	}
	else {
		DEBUG_IP("Using static IP", conf.localAddr, conf.netmask, conf.gateway);
		return false;
	}
}

#endif // WLAN_H
