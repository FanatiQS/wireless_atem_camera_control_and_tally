#include <stdbool.h> // true

#include <lwip/udp.h> // struct udp_pcb
#include <lwip/tcpip.h> // LOCK_TCPIO_CORE, UNLCOK_TCPIP_CORE
#include <lwip/init.h> // LWIP_VERSION_STRING

#include "./user_config.h" // DEBUG_TALLY, DEBUG_CC
#include "./debug.h" // DEBUG_PRINTF, DEBUG_IP, WRAP
#include "./atem_sock.h" // atem_init
#include "./http.h" // config_t, CONF_FLAG_STATICIP, http_init
#include "./init.h" // FIRMWARE_VERSION_STRING
#include "./dns.h" // captive_portal_init

#ifdef ESP8266

#include <user_interface.h> // wifi_set_opmode_current, STATIONAP_MODE, wifi_station_set_reconnect_policy, station_config, wifi_station_get_config, wifi_set_event_handler_cb, wifi_station_connect, wifi_station_dchpc_stop, wifi_set_ip_info, STATION_IF, ip_info
#include <spi_flash.h> // spi_flash_read, SPI_FLASH_RESULT_OK, spi_flash_get_id
#include <version.h> // ESP_SDK_VERSION_STRING
#ifdef ARDUINO
#include <core_version.h> // ARDUINO_ESP8266_GIT_DESC
#endif // ARDUINO

// Sets configuration to use last memory page of flash before system data, same as arduino uses for EEPROM
#define FLASH_SIZE (1 << ((spi_flash_get_id() >> 16) & 0xff))
#define CONFIG_START (FLASH_SIZE - (12 + 4 + 4) * 1024)

// Disables scanning for wifi station when configuration network is used
static void network_callback(System_Event_t* event) {
	switch (event->event) {
		// Disables scanning for station when connection is made to soft ap
		case EVENT_SOFTAPMODE_STACONNECTED: {
			if (wifi_softap_get_station_num() == 0) break;
			DEBUG_PRINTF("Disabled wifi station\n");
			if (!wifi_station_disconnect()) {
				DEBUG_PRINTF("Failed to disable wifi station\n");
				break;
			}
			break;
		}
		// Re enables scanning for station when soft ap is unused
		case EVENT_SOFTAPMODE_STADISCONNECTED: {
			if (wifi_softap_get_station_num()) break;
			DEBUG_PRINTF("Enabled wifi station\n");
			if (!wifi_station_connect()) {
				DEBUG_PRINTF("Failed to enable wifi station\n");
				break;
			}
			break;
		}
	}
}

#else // ESP8266

#error No WiFi implementation for platform

#endif // ESP8266



// Initializes firmware
static void _waccat_init() {
	struct config_t conf;

	DEBUG_PRINTF(
		"\n\n"
		"Booting...\n"

		// Prints firmware version
		"Firmware version: " FIRMWARE_VERSION_STRING "\n"

		// Prints LwIP firmware version
		"Using LwIP version: " LWIP_VERSION_STRING "\n"

#ifdef ESP8266
		"Using ESP8266 SDK version: " ESP_SDK_VERSION_STRING "\n"
#ifdef ARDUINO
		"Using Arduino SDK version: " WRAP(ARDUINO_ESP8266_GIT_DESC) "\n"
#endif // ARDUINO
#endif // ESP8266

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

	// Initializes captive portal
	if (!captive_portal_init()) {
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
	if (!wifi_station_get_config(&stationConfig)) {
		DEBUG_PRINTF("Failed to get Station configuration\n");
		return;
	}
	DEBUG_PRINTF(
		"Station SSID: \"%.*s\"\n"
		"Station PSK \"%.*s\"\n",
		(int)sizeof(stationConfig.ssid), stationConfig.ssid,
		(int)sizeof(stationConfig.password), stationConfig.password
	);
#endif // DEBUG

	// Disables station scanning when soft ap is used to fix soft ap stability issue
	wifi_set_event_handler_cb(network_callback);

	// Starts connecting to WiFi station
	if (!wifi_station_connect()) {
		DEBUG_PRINTF("Failed to start wifi connection\n");
		return;
	}

	// Reads configuration from non-volotile flash memory
	if (spi_flash_read(CONFIG_START, (uint32_t*)&conf, sizeof(conf)) != SPI_FLASH_RESULT_OK) {
		DEBUG_PRINTF("Failed to read device configuration\n");
		return;
	}
#endif // ESP8266

	// Uses static IP if configured to do so
	if (conf.flags & CONF_FLAG_STATICIP) {
		DEBUG_IP("Using static IP", conf.localAddr, conf.netmask, conf.gateway);

		// Sets static ip
#ifdef ESP8266
		struct ip_info ipInfo = {
			.ip.addr = conf.localAddr,
			.netmask.addr = conf.netmask,
			.gw.addr = conf.gateway
		};
		if (!wifi_station_dhcpc_stop()) {
			DEBUG_PRINTF("Failed to stop DHCP client\n");
			return;
		}
		if (!wifi_set_ip_info(STATION_IF, &ipInfo)) {
			DEBUG_PRINTF("Failed to set static ip\n");
			return;
		}
#endif // ESP8266
	}
	else {
		DEBUG_PRINTF("Using DHCP\n");
	}

	// Initializes connection to ATEM
	struct udp_pcb* pcb = atem_init(conf.atemAddr, conf.dest);
	if (pcb == NULL) {
		DEBUG_PRINTF("Boot failed\n");
	}
	else {
		DEBUG_PRINTF("Boot completed\n");
	}
}

// Initilization wrapper to handle multithreaded platforms
void waccat_init() {
	// Required when LwIP core is running in another thread
#ifndef NO_SYS
	LOCK_TCPIP_CORE();
#endif // NO_SYS

	// Initializes firmware
	_waccat_init();

	// Required when LwIP core is running in another thread
#ifndef NO_SYS
	UNLOCK_TCPIP_CORE();
#endif // NO_SYS
}
