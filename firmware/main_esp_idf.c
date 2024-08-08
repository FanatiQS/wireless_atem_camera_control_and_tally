#include <stddef.h> // NULL
#include <stdlib.h> // abort

#include <lwip/init.h> // LWIP_VERSION
#include <esp_wifi.h> // WIFI_INIT_CONFIG_DEFAULT, esp_wifi_init, esp_wifi_set_mode, esp_wifi_start, esp_wifi_connect, esp_wifi_get_config, wifi_init_config_t
#include <esp_wifi_types.h> // WIFI_MODE_APSTA, WIFI_IF_STA, WIFI_IF_AP, wifi_config_t, WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED
#include <esp_netif.h> // esp_netif_init, esp_netif_set_hostname, esp_netif_set_ip_info, esp_netif_dhcpc_stop
#include <esp_netif_types.h> // esp_netif_t, esp_netif_ip_info_t
#include <esp_netif_defaults.h> // esp_netif_create_default_wifi_ap, esp_netif_create_default_wifi_sta
#include <esp_event.h> // esp_event_loop_create_default, esp_event_handler_instance_register
#include <esp_err.h> // ESP_OK, ESP_ERROR_CHECK, esp_err_t
#include <nvs_flash.h> // nvs_flash_init
#include <esp_idf_version.h> // ESP_IDF_VERSION, ESP_IDF_VERSION_VAL
#if ARDUINO
#include <core_version.h> // ARDUINO_ESP32_GIT_DESC, ARDUINO_ESP32_GIT_VER
#endif // ARDUINO

#include "./user_config.h" // DEBUG
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, WRAP, DEBUG_BOOT_INFO, DEBUG_WLAN
#include "./atem_sock.h" // atem_init
#include "./http.h" // http_init
#include "./flash.h" // struct flash_config, flash_config_read
#include "./dns.h" // captive_portal_init
#include "./wlan.h" // wlan_station_dhcp_get



// Verifies LwIP version
#if LWIP_VERSION != 0x02010300
#warning Expected LwIP version 2.1.3
#endif // LWIP_VERSION

// Verifies versions of ESP8266 SDK and Arduino
#if ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 2, 1)
#warning Expected ESP-IDF version 5.2.1
#endif // ESP_IDF_VERSION
#if ARDUINO && !defined(ARDUINO_ESP32_RELEASE_2_0_4)
#warning Expected Arduino ESP32 version 2.0.4
#endif // ARDUINO && ARDUINO_ESP32_RELEASE_2_0_4)

// Debug boot line for arduino version
#if ARDUINO
#define DEBUG_BOOT_VERSION_ARDUINO "Using Arduino ESP32 SDK version: " WRAP(ARDUINO_ESP32_GIT_DESC) "\n"
#else // ARDUINO
#define DEBUG_BOOT_VERSION_ARDUINO ""
#endif // ARDUINO



// Automatically reconnects wlan if disconnected
static void wlan_reconnect_callback() {
	esp_err_t err = esp_wifi_connect();
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to reconnect wlan station: %x\n", err);
	}
}

// Initializes firmware
#if ARDUINO
void waccat_init(void) {
#else // ARDUINO
void app_main(void) {
#endif // ARDUINO
	esp_err_t err;

	DEBUG_PRINTF(
		DEBUG_BOOT_INFO
		"Using ESP_IDF version: " IDF_VER "\n"
		"Using ESP target: " CONFIG_IDF_TARGET "\n"
		DEBUG_BOOT_VERSION_ARDUINO
	);

	// Initializes wlan and its underlying components
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_ap();
	esp_netif_t* netif_sta = esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// Initializes the HTTP configuration server
	if (!http_init()) {
		abort();
	}

	// Initializes captive portal
	captive_portal_init();

	// Enables handling softap connections even if the initilization fails further down
	ESP_ERROR_CHECK(esp_wifi_start());

	// Enable wlan station along with existing softap
	err = esp_wifi_set_mode(WIFI_MODE_APSTA);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to set wifi opmode: %x\n", err);
		return;
	}

	// Gets WiFi softap SSID and PSK for debug printing and hostname
	wifi_config_t conf_ap;
	err = esp_wifi_get_config(WIFI_IF_AP, &conf_ap);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read soft ap configuration: %x\n", err);
	}
	else {
		DEBUG_WLAN("Soft AP", conf_ap.ap.ssid, conf_ap.ap.password);

		// Terminates max length device name, nothing else in the struct is safe to use after this
		((char*)conf_ap.ap.ssid)[sizeof(conf_ap.ap.ssid)] = '\0';

		// Sets hostname of device
		err = esp_netif_set_hostname(netif_sta, (char*)conf_ap.ap.ssid);
		if (err != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to set hostname: %x\n", err);
		}
	}

#if DEBUG
	// Gets WiFi station SSID and PSK for debug printing
	wifi_config_t conf_sta;
	err = esp_wifi_get_config(WIFI_IF_STA, &conf_sta);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to get Station configuration: %x\n", err);
	}
	else {
		DEBUG_WLAN("Station", conf_sta.sta.ssid, conf_sta.sta.password);
	}
#endif // DEBUG

	// Sets WiFi to automatically reconnect when connection is lost
	err = esp_event_handler_instance_register(
		WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
		&wlan_reconnect_callback,
		NULL, NULL
	);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to register wlan reconnect callback\n");
	}

	// Reads configuration from non-volotile flash memory
	struct flash_config conf;
	if (!flash_config_read(&conf)) {
		return;
	}

	// Uses static IP if configured to do so
	if (!wlan_station_dhcp_get(&conf)) {
		esp_netif_ip_info_t ip_info = {
			.ip.addr = conf.localip,
			.netmask.addr = conf.netmask,
			.gw.addr = conf.gateway
		};
		err = esp_netif_dhcpc_stop(netif_sta);
		if (err != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to stop DHCP client: %x\n", err);
			return;
		}
		err = esp_netif_set_ip_info(netif_sta, &ip_info);
		if (err != ESP_OK) {
			DEBUG_ERR_PRINTF("Failed to set static ip: %x\n", err);
			return;
		}
	}

	// Starts connecting to WiFi station
	err = esp_wifi_connect();
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to start wifi connection: %x\n", err);
		return;
	}

	// Initializes connection to ATEM
	if (atem_init(conf.atem_addr, conf.dest) == NULL) {
		return;
	}

	DEBUG_PRINTF("Boot completed\n");
}
