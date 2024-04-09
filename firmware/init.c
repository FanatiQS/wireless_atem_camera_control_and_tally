#include <stdbool.h> // true

#include <lwip/init.h> // LWIP_VERSION
#include <user_interface.h> // wifi_set_opmode_current, STATIONAP_MODE, wifi_station_set_reconnect_policy, struct station_config, wifi_station_get_config, wifi_set_event_handler_cb, wifi_station_connect, wifi_station_dchpc_stop, wifi_set_ip_info, STATION_IF, ip_info, uart_div_modify, struct softap, wifi_softap_get_config, wifi_station_set_hostname
#include <version.h> // ESP_SDK_VERSION_STRING, ESP_SDK_VERSION_NUMBER
#include <eagle_soc.h> // UART_CLK_FREQ, WRITE_PERI_REG, PERIPHS_IO_MUX_U0TXD_U
#if ARDUINO
#include <core_version.h> // ARDUINO_ESP8266_GIT_DESC, ARDUINO_ESP8266_GIT_VER
#endif // ARDUINO

#include "./user_config.h" // DEBUG, VERSIONS_ANY
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, WRAP, DEBUG_BOOT_INFO
#include "./atem_sock.h" // atem_init
#include "./http.h" // http_init
#include "./flash.h" // struct config_t, flash_config_read
#include "./dns.h" // captive_portal_init
#include "./wlan.h" // wlan_station_dhcp_get



// Debug info of the SDK version
#if ARDUINO
#define DEBUG_BOOT_VERSION_ARDUINO "Using Arduino SDK version: " WRAP(ARDUINO_ESP8266_GIT_DESC) "\n"
#else // ARDUINO
#define DEBUG_BOOT_VERSION_ARDUINO ""
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

// Verifies versions of ESP8266 SDK and Arduino
#if !VERSIONS_ANY
#if ESP_SDK_VERSION_NUMBER != 0x020200
#error Expected ESP8266 SDK version 2.2.0
#endif // ESP_SDK_VERSION_NUMBER
#if ARDUINO && ARDUINO_ESP8266_GIT_VER != 0x9c56ed1f
#error Expected Arduino ESP8266 version 2.7.0
#endif // ARDUINO_ESP8266_GIT_VER
#endif // !VERSIONS_ANY

// Verifies LwIP version
#if !VERSIONS_ANY
#if LWIP_VERSION != 0x020102ff
#error Expected LwIP version 2.1.2
#endif // LWIP_VERSION
#endif // !VERSIONS_ANY



// Sets default uart baud rate if not specified
#ifndef DEBUG_BAUD
#define DEBUG_BAUD 115200
#endif // DEBUG_BAUD



// Initializes firmware
void waccat_init(void) {
	// Initializes uart serial printing
#if DEBUG
	WRITE_PERI_REG(PERIPHS_IO_MUX_U0TXD_U, 0);
	uart_div_modify(0, UART_CLK_FREQ / DEBUG_BAUD);
#endif // DEBUG

	DEBUG_PRINTF(
		DEBUG_BOOT_INFO
		"Using ESP8266 NON-OS SDK version: " ESP_SDK_VERSION_STRING "\n"
		DEBUG_BOOT_VERSION_ARDUINO
	);

	// Initializes the HTTP configuration server
	if (!http_init()) {
		return;
	}

	// Initializes captive portal
	captive_portal_init();

	// Enables both station mode and ap mode without writing to flash
	if (!wifi_set_opmode_current(STATIONAP_MODE)) {
		DEBUG_ERR_PRINTF("Failed to set wifi opmode\n");
		return;
	}

	// Gets WiFi softap SSID and PSK for debug printing and hostname
	struct softap_config conf_ap;
	if (!wifi_softap_get_config(&conf_ap)) {
		DEBUG_ERR_PRINTF("Failed to read soft ap configuration\n");
	}
	else {
		DEBUG_WLAN("Soft AP", conf_ap.ssid, conf_ap.password);

		// Terminates max length device name, nothing else in the struct is safe to use after this
		((char*)conf_ap.ssid)[sizeof(conf_ap.ssid)] = '\0';

		// Sets hostname of device
		if (!wifi_station_set_hostname((char*)conf_ap.ssid)) {
			DEBUG_ERR_PRINTF("Failed to set hostname\n");
		}
	}

#if DEBUG
	// Gets WiFi station SSID and PSK for debug printing
	struct station_config conf_sta;
	if (!wifi_station_get_config(&conf_sta)) {
		DEBUG_ERR_PRINTF("Failed to get Station configuration\n");
	}
	else {
		DEBUG_WLAN("Station", conf_sta.ssid, conf_sta.password);
	}
#endif // DEBUG

	// Sets WiFi to automatically reconnect when connection is lost
	if (!wifi_station_set_reconnect_policy(true)) {
		DEBUG_ERR_PRINTF("Failed to set reconnect policy\n");
	}

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

	// Initializes connection to ATEM
	if (!atem_init(conf.atemAddr, conf.dest)) {
		return;
	}

	DEBUG_PRINTF("Boot completed\n");
}
