#include <stdbool.h> // true

#include <lwip/udp.h> // struct udp_pcb
#include <lwip/tcpip.h> // LOCK_TCPIO_CORE, UNLCOK_TCPIP_CORE
#include <lwip/init.h> // LWIP_VERSION_STRING

#include "./user_config.h" // DEBUG, DEBUG_TALLY, DEBUG_CC, DEBUG_HTTP
#include "./debug.h" // DEBUG_PRINTF, DEBUG_IP, WRAP
#include "./atem_sock.h" // atem_init
#include "./http.h" // http_init
#include "./flash.h" // struct config_t, CONF_FLAG_STATICIP, flash_config_read
#include "./init.h" // FIRMWARE_VERSION_STRING
#include "./dns.h" // captive_portal_init

// Defines empty LWIP thread locking macros when LWIP is not running in a separate thread
#if NO_SYS
#define LOCK_TCPIP_CORE()
#define UNLOCK_TCPIP_CORE()
#endif // NO_SYS



#ifdef ESP8266

#include <user_interface.h> // wifi_set_opmode_current, STATIONAP_MODE, wifi_station_set_reconnect_policy, struct station_config, wifi_station_get_config, wifi_set_event_handler_cb, wifi_station_connect, wifi_station_dchpc_stop, wifi_set_ip_info, STATION_IF, ip_info, uart_div_modify, struct softap, wifi_softap_get_config
#include <version.h> // ESP_SDK_VERSION_STRING
#include <eagle_soc.h> // UART_CLK_FREQ, WRITE_PERI_REG, PERIPHS_IO_MUX_U0TXD_U
#if ARDUINO
#include <core_version.h> // ARDUINO_ESP8266_GIT_DESC
#endif // ARDUINO

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

// Debug info of the SDK version
#define BOOT_INFO_VERSION_ESP "Using ESP8266 SDK version: " ESP_SDK_VERSION_STRING "\n"
#if ARDUINO
#define BOOT_INFO_VERSION_ARDUINO "Using Arduino SDK version: " WRAP(ARDUINO_ESP8266_GIT_DESC) "\n"
#endif // ARDUINO
#define BOOT_INFO_VERSIONS BOOT_INFO_VERSION_ESP BOOT_INFO_VERSION_ARDUINO

#else // ESP8266

#error No WiFi implementation for platform

#endif // ESP8266



// Defines string label of DEBUG_TALLY flag
#if DEBUG_TALLY
#define DEBUG_TALLY_LABEL "enabled"
#else // DEBUG_TALLY
#define DEBUG_TALLY_LABEL "disabled"
#endif // DEBUG_TALLY

// Defines string label of DEBUG_CC flag
#if DEBUG_CC
#define DEBUG_CC_LABEL "enabled"
#else // DEBUG_CC
#define DEBUG_CC_LABEL "disabled"
#endif // DEBUG_CC

// Defines string label of DEBUG_ATEM flag
#if DEBUG_ATEM
#define DEBUG_ATEM_LABEL "enabled"
#else // DEBUG_ATEM
#define DEBUG_ATEM_LABEL "disabled"
#endif // DEBUG_ATEM

// Defines string label of DEBUG_HTTP flag
#if DEBUG_HTTP
#define DEBUG_HTTP_LABEL "enabled"
#else // DEBUG_ATEM
#define DEBUG_HTTP_LABEL "disabled"
#endif // DEBUG_HTTP



// Sets default uart baud rate if not specified
#ifndef DEBUG_BAUD
#define DEBUG_BAUD 115200
#endif // DEBUG_BAUD



// Initializes firmware
static void _waccat_init(void) {
	struct config_t conf;

	// Initializes uart serial printing
#if DEBUG
#ifdef ESP8266
	WRITE_PERI_REG(PERIPHS_IO_MUX_U0TXD_U, 0);
	uart_div_modify(0, UART_CLK_FREQ / DEBUG_BAUD);
#endif // ESP8266
#endif // DEBUG

	DEBUG_PRINTF(
		"\n\n"
		"Booting...\n"

		// Prints versions
		"Firmware version: " FIRMWARE_VERSION_STRING "\n"
		"Using LwIP version: " LWIP_VERSION_STRING "\n"
		BOOT_INFO_VERSIONS

		// Prints debugging flags enabled/disabled state
		"Tally debug: " DEBUG_TALLY_LABEL "\n"
		"Camera control debug: " DEBUG_CC_LABEL "\n"
		"ATEM debug: " DEBUG_ATEM_LABEL "\n"
		"HTTP debug: " DEBUG_HTTP_LABEL "\n"
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

	// Reads configuration from non-volotile flash memory
	if (!flash_config_read(&conf)) {
		return;
	}

#ifdef ESP8266
	// Gets WiFi softap SSID and PSK for debug printing
	struct softap_config softapConfig;
	if (!wifi_softap_get_config(&softapConfig)) {
		DEBUG_PRINTF("Failed to read soft ap configuration\n");
		return false;
	}
	DEBUG_PRINTF(
		"Soft AP SSID: \"%.*s\"\n"
		"Soft AP PSK: \"%.*s\"\n"
		"Soft AP Channel: %d\n",
		softapConfig.ssid_len, softapConfig.ssid,
		(int)sizeof(softapConfig.password), softapConfig.password,
		softapConfig.channel
	);

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
void waccat_init(void) {
	// Required when LwIP core is running in another thread
	LOCK_TCPIP_CORE();

	// Initializes firmware
	_waccat_init();

	// Required when LwIP core is running in another thread
	UNLOCK_TCPIP_CORE();
}
