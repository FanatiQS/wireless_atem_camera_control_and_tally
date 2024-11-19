#include <stdlib.h> // EXIT_FAILURE
#include <stddef.h> // NULL
#include <assert.h> // assert

#include <sys/utsname.h> // struct utsname, uname
#include <lwip/init.h> // LWIP_VERSION, lwip_init
#include <lwip/netif.h> // struct netif, netif_add_noaddr, netif_add, netif_input, netif_set_hostname, netif_set_default, netif_set_up
#include <lwip/ip4_addr.h> // ip4_addr_t
#include <lwip/err.h> // ERR_OK
#include <lwip/dhcp.h> // dhcp_start
#include <lwip/timeouts.h> // sys_check_timeouts
#include <netif/tapif.h> // tapif_init, tapif_poll

#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, DEBUG_BOOT_INFO, DEBUG_WLAN
#include "./atem_sock.h" // atem_init
#include "./http.h" // http_init
#include "./flash.h" // struct cache_t, flash_cache_read
#include "./dns.h" // captive_portal_init
#include "./wlan.h" // wlan_station_dhcp_get



// Verifies LwIP version
#if LWIP_VERSION != 0x02020100
#warning Expected LwIP version 2.2.1
#endif // LWIP_VERSION

// @todo should be disabled with NO_WIFI_SOFTAP macro instead?
void wlan_softap_disable(void) {}

// @todo should have implementation here
#include <stdint.h> // int8_t
int8_t wlan_station_rssi(void) { return 0; }



// Initializes firmware
int main(void) {
	struct netif netif = {0};

	// Initializes LwIP network stack
	lwip_init();

#if DEBUG
	struct utsname os;
	assert(uname(&os) == 0);
#endif // DEBUG
	DEBUG_PRINTF(
		DEBUG_BOOT_INFO
		"Host system: %s\n", os.version
	);

	// Initializes the HTTP configuration server
	if (!http_init()) {
		return EXIT_FAILURE;
	}

	// Initializes captive portal
	captive_portal_init();

	// Reads configuration from file
	struct flash_cache cache;
	if (!flash_cache_read(&cache)) {
		return EXIT_FAILURE;
	}

	// Sets hostname of device
	netif_set_hostname(&netif, cache.wlan.name);

	DEBUG_WLAN("Station", cache.wlan.ssid, cache.wlan.psk);

	// Initializes TUN/TAP interface with DHCP or static IP
	if (wlan_station_dhcp_get(&cache.config)) {
		if (netif_add_noaddr(&netif, NULL, tapif_init, netif_input) == NULL) {
			DEBUG_ERR_PRINTF("Failed to initialize netif with DHCP\n");
			return EXIT_FAILURE;
		}
		if (dhcp_start(&netif) != ERR_OK) {
			DEBUG_ERR_PRINTF("Failed to start DHCP server\n");
			return EXIT_FAILURE;
		}
	}
	else {
		const ip4_addr_t localip = { cache.config.localip };
		const ip4_addr_t netmask = { cache.config.netmask };
		const ip4_addr_t gateway = { cache.config.gateway };
		if (netif_add(&netif, &localip, &netmask, &gateway, NULL, tapif_init, netif_input) == NULL) {
			DEBUG_ERR_PRINTF("Failed to initialize netif with static IP\n");
			return EXIT_FAILURE;
		}
	}

	// Initializes connection to ATEM
	if (!atem_init(cache.config.atem_addr, cache.config.dest)) {
		return EXIT_FAILURE;
	}

	DEBUG_PRINTF("Boot completed\n");

	// Runs event loop with TUN/TAP interface
	netif_set_default(&netif);
	netif_set_up(&netif);
	while (1) {
		sys_check_timeouts();
		tapif_select(&netif);
	}
}
