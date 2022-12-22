#include <stdbool.h> // true

#include <lwip/udp.h> // struct udp_pcb
#include <lwip/tcpip.h> // LOCK_TCPIO_CORE, UNLCOK_TCPIP_CORE

#include "./user_config.h" // DEBUG_TALLY, DEBUG_CC
#include "./debug.h" // DEBUG_PRINTF
#include "./udp.h" // atem_udp_init
#include "./http.h" // config_t, CONF_FLAG_STATICIP, http_init
#include "./init.h" // FIRMWARE_VERSION_STRING

#ifdef ESP8266

#include <user_interface.h> // wifi_set_opmode_current, STATIONAP_MODE, wifi_station_set_reconnect_policy, station_config, wifi_station_get_config, wifi_set_event_handler_cb, wifi_station_connect, wifi_station_dchpc_stop, wifi_set_ip_info, STATION_IF, ip_info

// @todo this relies on Arduino
extern uint32_t _EEPROM_start;
#define CONFIG_START ((uint32_t)&_EEPROM_start - 0x40200000)

#else // ESP8266

#error No WiFi implementation for platform

#endif // ESP8266



// Initializes ATEM connection
static void _atem_init() {
	struct config_t conf;

	DEBUG_PRINTF(
		"\n\n"
		"Booting...\n"

		// Prints firmware version
		"Firmware version: " FIRMWARE_VERSION_STRING "\n"

		// Prints LwIP firmware version
		"Using LwIP version: " LWIP_VERSION_STRING "\n"

		// Prints debugging flags enabled/disabled state
		"Tally debug: "
#if DEBUG_TALLY
		"enabled"
#else // DEBUG_TALLY
		"disabled"
#endif // DEBUG_TALLY
		"\n"
		"Camera control debug: "
#if DEBUG_CC
		"enabled"
#else // DEBUG_CC
		"disabled"
#endif // DEBUG_CC
		"\n"
	);

#ifdef ESP8266
	// Enables both station mode and ap mode without writing to flash
	if (!wifi_set_opmode_current(STATIONAP_MODE)) {
		DEBUG_PRINTF("Failed to set wifi opmode\n");
		return;
	}
#endif // ESP8266

	// Initializes the HTTP configuration server
	if (!http_init()) {
		return;
	}

#ifdef ESP8266
	// Sets WiFi to automatically reconnect when connection is lost
	if (!wifi_station_set_reconnect_policy(true)) {
		DEBUG_PRINTF("Failed to set reconnect policy\n");
		return;
	}

#if DEBUG
	// Gets WiFi station SSID and PSK for debug printing
	struct station_config stationConfig;
	wifi_station_get_config(&stationConfig);
	DEBUG_PRINTF(
		"Station SSID: \"%.*s\"\n"
		"Station PSK \"%.*s\"\n",
		sizeof(stationConfig.ssid), stationConfig.ssid,
		sizeof(stationConfig.password), stationConfig.password
	);
#endif // DEBUG

	// Starts connecting to WiFi station
	if (!wifi_station_connect()) {
		DEBUG_PRINTF("Failed to start wifi connection\n");
		return;
	}

	// Reads configuration from non-volotile flash memory
	spi_flash_read(CONFIG_START, (uint32_t*)&conf, sizeof(conf));
#endif // ESP8266

	// @todo
	if (conf.flags & CONF_FLAG_STATICIP) {
		DEBUG_PRINTF(
			"Using static IP:\n"
			"\tLocal address: %s\n"
			"\tSubnet mask: %s\n"
			"\tGateway: %s\n",
			ipaddr_ntoa(&conf.localAddr),
			ipaddr_ntoa(&conf.netmask), 
			ipaddr_ntoa(&conf.gateway)
		);

		// Sets static ip
#ifdef ESP8266
		struct ip_info ipInfo = {
			ip: conf.localAddr,
			netmask: conf.netmask,
			gw: conf.gateway
		};
		wifi_station_dhcpc_stop();
		wifi_set_ip_info(STATION_IF, &ipInfo);
#endif // ESP8266
	}
	else {
		DEBUG_PRINTF("Using DHCP\n");
	}

	// Initializes connection to ATEM
	struct udp_pcb* pcb = atem_udp_init(conf.atemAddr, conf.dest);
	if (pcb == NULL) return;

	DEBUG_PRINTF("Sync boot complete\n");
}

// Initilization wrapper to handle multithreaded platforms
void atem_init() {
	// Required when LwIP core is running in another thread
#ifndef NO_SYS
	LOCK_TCPIP_CORE();
#endif // NO_SYS

	// @todo
	_atem_init();

	// Required when LwIP core is running in another thread
#ifndef NO_SYS
	UNLOCK_TCPIP_CORE();
#endif // NO_SYS
}
