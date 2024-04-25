#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "./src/firmware/init.h" // waccat_init

void setup() {
	// Sets softap password if not configured
	if (WiFi.softAPPSK().isEmpty()) {
		WiFi.persistent(true);
		WiFi.softAP(NULL, WiFi.macAddress().c_str());
	}

	// Initializes wifi, ATEM connection, SDI shield and GPIO LEDs
	WiFi.begin();
	waccat_init();

	// Sets up OTA update server
	ArduinoOTA.setHostname(WiFi.softAPSSID().c_str());
	ArduinoOTA.begin();

	// Adds mDNS querying support
	MDNS.begin(WiFi.softAPSSID());
	MDNS.addService("http", "tcp", 80);
}

void loop() {
	MDNS.update();
	ArduinoOTA.handle();
}
