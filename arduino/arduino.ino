#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#endif // ESP8266

#ifdef ESP32
#include <esp_wifi.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#endif // ESP32

#include <ArduinoOTA.h>

#include "./src/waccat.h" // waccat_init

void setup() {
#ifdef ESP8266
	// Sets softap password if not configured
	if (WiFi.softAPPSK().isEmpty()) {
		WiFi.persistent(true);
		WiFi.softAP(NULL, WiFi.macAddress().c_str());
	}
#endif // ESP8266

#ifdef ESP32
	// Sets softap password if not configured
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	wifi_config_t conf_ap;
	ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_AP, &conf_ap));
	if (conf_ap.ap.authmode == WIFI_AUTH_OPEN) {
		WiFi.macAddress().toCharArray((char*)conf_ap.ap.password, sizeof(conf_ap.ap.password));
		conf_ap.ap.authmode = WIFI_AUTH_WPA2_PSK;
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &conf_ap));
	}
#endif // ESP32

	// Initializes wifi, ATEM connection, SDI shield and GPIO LEDs
	waccat_init();

	// Initializes OTA server with waccat configured name
	ArduinoOTA.setHostname(WiFi.softAPSSID().c_str());
	ArduinoOTA.begin();

	// Initializes mDNS lookup with waccat configured name
	MDNS.begin(WiFi.softAPSSID().c_str());
	MDNS.addService("http", "tcp", 80);
}

void loop() {
#ifdef ESP8266
	MDNS.update();
#endif // ESP8266
	ArduinoOTA.handle();
}
