#include <stdbool.h> // true

#include <lwip/udp.h> // struct udp_pcb
#include <lwip/tcpip.h> // LOCK_TCPIO_CORE, UNLCOK_TCPIP_CORE
#include <lwip/init.h> // LWIP_VERSION_STRING, LWIP_VERSION

#include "./user_config.h" // DEBUG, DEBUG_TALLY, DEBUG_CC, DEBUG_HTTP, VERSIONS_ANY
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, WRAP
#include "./atem_sock.h" // atem_init
#include "./http.h" // http_init
#include "./flash.h" // struct config_t, flash_config_read
#include "./init.h" // FIRMWARE_VERSION_STRING
#include "./dns.h" // captive_portal_init
#include "./wlan.h" // wlan_station_dhcp_get



#ifdef ESP8266
#include <string.h> // strncpy
#include <user_interface.h> // wifi_set_opmode_current, STATIONAP_MODE, wifi_station_set_reconnect_policy, struct station_config, wifi_station_get_config, wifi_set_event_handler_cb, wifi_station_connect, wifi_station_dchpc_stop, wifi_set_ip_info, STATION_IF, ip_info, uart_div_modify, struct softap, wifi_softap_get_config, wifi_station_set_hostname
#include <version.h> // ESP_SDK_VERSION_STRING, ESP_SDK_VERSION_NUMBER
#include <eagle_soc.h> // UART_CLK_FREQ, WRITE_PERI_REG, PERIPHS_IO_MUX_U0TXD_U
#if ARDUINO
#include <core_version.h> // ARDUINO_ESP8266_GIT_DESC, ARDUINO_ESP8266_GIT_VER
#endif // ARDUINO

// Disables scanning for wifi station when configuration network is used
static void network_callback(System_Event_t* event) {
	switch (event->event) {
		// Disables scanning for station when connection is made to soft ap
		case EVENT_SOFTAPMODE_STACONNECTED: {
			if (wifi_station_get_connect_status() == STATION_GOT_IP) break;
			DEBUG_PRINTF("Disabled wifi station\n");
			if (!wifi_station_disconnect()) {
				DEBUG_ERR_PRINTF("Failed to disable wifi station\n");
				break;
			}
			break;
		}
		// Re enables scanning for station when soft ap is unused
		case EVENT_SOFTAPMODE_STADISCONNECTED: {
			if (wifi_softap_get_station_num()) break;
			if (wifi_station_get_connect_status() == STATION_GOT_IP) break;
			DEBUG_PRINTF("Enabled wifi station\n");
			if (!wifi_station_connect()) {
				DEBUG_ERR_PRINTF("Failed to enable wifi station\n");
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
#else // ARDUINO
#define BOOT_INFO_VERSION_ARDUINO ""
#endif // ARDUINO
#define BOOT_INFO_VERSIONS BOOT_INFO_VERSION_ESP BOOT_INFO_VERSION_ARDUINO

// Verifies versions of ESP8266 SDK and Arduino
#if !VERSIONS_ANY
#if ESP_SDK_VERSION_NUMBER != 0x020200
#error Expected ESP8266 SDK version 2.2.0
#endif // ESP_SDK_VERSION_NUMBER
#if ARDUINO && ARDUINO_ESP8266_GIT_VER != 0x9c56ed1f
#error Expected Arduino ESP8266 version 2.7.0
#endif // ARDUINO_ESP8266_GIT_VER
#endif // !VERSIONS_ANY

#else

#error No WiFi implementation for platform

#endif



// Verifies LwIP version
#if !VERSIONS_ANY
#if LWIP_VERSION != 0x020102ff
#error Expected LwIP version 2.1.2
#endif // LWIP_VERSION
#endif // !VERSIONS_ANY



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

// Defines string label of DEBUG_DNS flag
#if DEBUG_DNS
#define DEBUG_DNS_LABEL "enabled"
#else // DEBUG_ATEM
#define DEBUG_DNS_LABEL "disabled"
#endif // DEBUG_DNS

// Defines undefined version for compilers where the macro does not exist
#ifndef __VERSION__
#define __VERSION__ "undefined"
#endif // __VERSION__



// Sets default uart baud rate if not specified
#ifndef DEBUG_BAUD
#define DEBUG_BAUD 115200
#endif // DEBUG_BAUD



// Initializes firmware
void waccat_init(void) {
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
		"Using compiler version: " __VERSION__ "\n"

		// Prints debugging flags enabled/disabled state
		"Tally debug: " DEBUG_TALLY_LABEL "\n"
		"Camera control debug: " DEBUG_CC_LABEL "\n"
		"ATEM debug: " DEBUG_ATEM_LABEL "\n"
		"HTTP debug: " DEBUG_HTTP_LABEL "\n"
		"DNS debug: " DEBUG_DNS_LABEL "\n"
	);

	// Initializes the HTTP configuration server
	if (!http_init()) {
		return;
	}

	// Initializes captive portal
	captive_portal_init();

#ifdef ESP8266
	// Gets WiFi softap SSID and PSK for debug printing and hostname
	struct softap_config conf_ap;
	if (!wifi_softap_get_config(&conf_ap)) {
		DEBUG_ERR_PRINTF("Failed to read soft ap configuration\n");
		return;
	}

	DEBUG_WLAN("Soft AP", conf_ap.ssid, conf_ap.password);

	// Enables both station mode and ap mode without writing to flash
	if (!wifi_set_opmode_current(STATIONAP_MODE)) {
		DEBUG_ERR_PRINTF("Failed to set wifi opmode\n");
		return;
	}

	// Terminates max length device name, nothing else in the struct is safe to use after this
	((char*)conf_ap.ssid)[sizeof(conf_ap.ssid)] = '\0';

	// Sets hostname of device
	if (!wifi_station_set_hostname((char*)conf_ap.ssid)) {
		DEBUG_ERR_PRINTF("Failed to set hostname\n");
	}

	// Sets WiFi to automatically reconnect when connection is lost
	if (!wifi_station_set_reconnect_policy(true)) {
		DEBUG_ERR_PRINTF("Failed to set reconnect policy\n");
		return;
	}

#if DEBUG
	// Gets WiFi station SSID and PSK for debug printing
	struct station_config conf_sta;
	if (!wifi_station_get_config(&conf_sta)) {
		DEBUG_ERR_PRINTF("Failed to get Station configuration\n");
		return;
	}
	else {
		DEBUG_WLAN("Station", conf_sta.ssid, conf_sta.password);
	}
#endif // DEBUG

	// Disables station scanning when soft ap is used to fix soft ap stability issue
	wifi_set_event_handler_cb(network_callback);

	// Reads configuration from non-volotile flash memory
	struct config_t conf;
	if (!flash_config_read(&conf)) {
		return;
	}

	// Uses static IP if configured to do so
	if (!wlan_station_dhcp_get(&conf)) {
		struct ip_info ipInfo = {
			.ip.addr = conf.localAddr,
			.netmask.addr = conf.netmask,
			.gw.addr = conf.gateway
		};
		if (!wifi_station_dhcpc_stop()) {
			DEBUG_ERR_PRINTF("Failed to stop DHCP client\n");
			return;
		}
		if (!wifi_set_ip_info(STATION_IF, &ipInfo)) {
			DEBUG_ERR_PRINTF("Failed to set static ip\n");
			return;
		}
	}

	// Starts connecting to WiFi station
	if (!wifi_station_connect()) {
		DEBUG_ERR_PRINTF("Failed to start wifi connection\n");
		return;
	}
#endif // ESP8266

	// Initializes connection to ATEM
	if (!atem_init(conf.atemAddr, conf.dest)) {
		return;
	}

	DEBUG_PRINTF("Boot completed\n");
}
