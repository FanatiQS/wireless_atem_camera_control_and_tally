#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

#include <lwip/init.h>

#include "html_template_engine.h"
#include "user_config.h"

#define LWIP_HDR_TCP_H // Fixes arduino and lwip collision
#undef FLASH_H
#include "./src/atem_sock.h" // atem_state
#include "./src/flash.h" // struct config_t, CONF_FLAG_STATICIP
#include "./src/init.h" // waccat_init, FIRMWARE_VERSION_STRING
#include "./src/debug.h" // WRAP, DEBUG_PRINTF



// Expected versions of libraries and SDKs
#define EXPECTED_LWIP_VERSION "2.1.2"
#define EXPECTED_ESP_VERSION "2.7.0"



// Only prints debug data when enabled from user_config.h
#if DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else // DEBUG
#define DEBUG_PRINT(arg) do {} while(0)
#define DEBUG_PRINTLN(arg) do {} while(0)
#endif // DEBUG



// Max length for name used in soft AP ssid and mdns lookup
#define NAME_MAX_LEN 16



// Configuration HTTP server and redirection DNS
ESP8266WebServer confServer(80);

// Names for all HTTP POST keys
#define KEY_SSID "ssid"
#define KEY_PSK "psk"
#define KEY_DEST "dest"
#define KEY_ATEMADDR "atem"
#define KEY_USESTATICIP "static"
#define KEY_LOCALIP "iplocal"
#define KEY_NETMASK "ipmask"
#define KEY_GATEWAY "ipgw"
#define KEY_NAME "name"

// Gets the analog voltage level calculated from voltage divider
#define RESOLUTION_MAX 1024
#define VMAX 3300
#define R1 300
#define R2 47
#define TRANSLATE_VOLTAGE(vout) (float)(VMAX * (R1 + R2) / R2 * vout / RESOLUTION_MAX) / 1000
#ifdef PIN_BATTREAD
#define HTML_VOLTAGE($, label, value) HTML_INFO($, label, TRANSLATE_VOLTAGE(value), 4, "%.1f", " v")
#else
#define HTML_VOLTAGE($, label, value)
#endif

// HTML configuration page template for use with html templating engine
#define HTML_CONFIG($, conf)\
	HTML($, "<!DOCTYPEhtml>"\
		"<meta charset=utf-8>"\
		"<meta content=\"width=device-width\"name=viewport>"\
		"<title>Configure Device</title>"\
		"<style>"\
			"body{display:flex;place-content:center}"\
			"form{background:#f8f8f8;padding:1em;border-radius:1em}"\
			"td{padding:0 1em}"\
			"tr:empty{height:1em}"\
			"input[maxlength]{width:100%%;margin:2px}"\
		"</style>"\
		"<form method=post><table>"\
	)\
	HTML_CURRENT_TIME($, "Request time")\
	HTML_TIME($, "Time since boot", time(NULL))\
	HTML_SPACER($)\
	HTML_ROW($, "Firmware Version") HTML($, FIRMWARE_VERSION_STRING)\
	HTML_RSSI($, "WiFi signal strength", WiFi.RSSI(), WiFi.isConnected())\
	HTML_INFO($, "ATEM connection status", atem_state, strlen(atem_state), "%s", "")\
	HTML_VOLTAGE($, "Voltage level", analogRead(PIN_BATTREAD))\
	HTML_SPACER($)\
	HTML_INPUT_TEXT($, "Name", WiFi.softAPSSID().c_str(), NAME_MAX_LEN, KEY_NAME)\
	HTML_SPACER($)\
	HTML_INPUT_TEXT($, "Network name (SSID)", WiFi.SSID().c_str(), WL_SSID_MAX_LENGTH, KEY_SSID)\
	HTML_INPUT_TEXT($, "Network password (PSK)", WiFi.psk().c_str(), WL_WPA_KEY_MAX_LENGTH, KEY_PSK)\
	HTML_SPACER($)\
	HTML_INPUT_NUMBER($, "Camera number", conf.dest, 254, 1, KEY_DEST)\
	HTML_INPUT_IP($, "ATEM IP", (uint32_t)conf.atemAddr, KEY_ATEMADDR)\
	HTML_SPACER($)\
	HTML_INPUT_CHECKBOX($, "Use Static IP", conf.flags, KEY_USESTATICIP)\
	HTML_INPUT_IP($, "Local IP", (uint32_t)WiFi.localIP(), KEY_LOCALIP)\
	HTML_INPUT_IP($, "Subnet mask", (uint32_t)WiFi.subnetMask(), KEY_NETMASK)\
	HTML_INPUT_IP($, "Gateway", (uint32_t)WiFi.gatewayIP(), KEY_GATEWAY)\
	HTML($, "</table><button style=\"margin:1em 2em\">Submit</button></form>")\
	HTML($, "<script>document.querySelector('form').action = location.origin + ':8080'</script>")

// Gets ip address from 4 HTML post fields as a single 32 bit int
#define IP_FROM_HTTP(server, name) ((server.arg(name "1").toInt()) |\
	(server.arg(name "2").toInt() << 8) | (server.arg(name "3").toInt() << 16) |\
	(server.arg(name "4").toInt() << 24))

// Handles HTTP GET and POST requests for configuration
void handleHTTP() {
	struct config_t confData;

	// Processes POST request
	if (confServer.method() == HTTP_POST) {
		// Sends response to post before switching network
 		confServer.send(200, "text/html",
			"<!DOCTYPEhtml><meta content=\"width=device-width\"name=viewport>"
			"<title>Configure Device</title>"
			"<div>Updated, device is rebooting..."
			"<div>Reload page when device has restarted"
			"<div>If device is unable to connect to wifi, it will "
				"create its own wireless network for configuration."
		);
		confServer.client().flush();

		DEBUG_PRINT("Received configuration over HTTP\n");

		// Sets WiFi ssid and psk
		WiFi.begin(confServer.arg(KEY_SSID), confServer.arg(KEY_PSK));
		DEBUG_PRINT("SSID: ");
		DEBUG_PRINTLN(WiFi.SSID());
		DEBUG_PRINT("PSK: ");
		DEBUG_PRINTLN(WiFi.psk());

		// Sets camera destination and IP address of ATEM
		confData.dest = confServer.arg(KEY_DEST).toInt();
		confData.atemAddr = IP_FROM_HTTP(confServer, KEY_ATEMADDR);
		DEBUG_PRINT("Camera ID: ");
		DEBUG_PRINTLN(confData.dest);
		DEBUG_PRINT("ATEM address: ");
		DEBUG_PRINTLN(IPAddress(confData.atemAddr));

		// Sets static IP data
		confData.flags = confServer.arg(KEY_USESTATICIP).equals("on");
		confData.localAddr = IP_FROM_HTTP(confServer, KEY_LOCALIP);
		confData.netmask = IP_FROM_HTTP(confServer, KEY_NETMASK);
		confData.gateway = IP_FROM_HTTP(confServer, KEY_GATEWAY);
		DEBUG_PRINT("Using static IP: ");
		DEBUG_PRINTLN((confData.flags) ? "TRUE" : "FALSE");
		DEBUG_PRINT("Local address: ");
		DEBUG_PRINTLN(IPAddress(confData.localAddr));
		DEBUG_PRINT("Subnet mask: ");
		DEBUG_PRINTLN(IPAddress(confData.netmask));
		DEBUG_PRINT("Gateway: ");
		DEBUG_PRINTLN(IPAddress(confData.gateway));

		// Sets device name used for soft ap and mdns
		WiFi.softAP(confServer.arg(KEY_NAME), WiFi.macAddress(), 1, 0, 1);
		DEBUG_PRINT("Name: ");
		DEBUG_PRINTLN(WiFi.softAPSSID());

		// Writes and commits configuration data to percist for next boot
		EEPROM.put(0, confData);
		EEPROM.commit();

		DEBUG_PRINT("Restarting...\n\n");

		// Restarts device to apply new configurations
		ESP.restart();
	}
	// Processes GET requests
	else {
		EEPROM.get(0, confData);
		char* html = (char*)malloc(HTML_GET_LEN(HTML_CONFIG));
		sprintf(html, HTML_BUILD(HTML_CONFIG, confData));
		confServer.send(200, "text/html", html);
		free(html);
	}
}



void setup() {
#if DEBUG
	Serial.begin(115200);

	// Prints mac address that is used as psk by soft AP
	DEBUG_PRINT("Mac address: ");
	DEBUG_PRINTLN(WiFi.macAddress());

	if (strcmp(LWIP_VERSION_STRING, EXPECTED_LWIP_VERSION)) {
		DEBUG_PRINT("WARNING: Expected LwIP version for this firmware: " EXPECTED_LWIP_VERSION "\n");
	}
	if (strcmp(WRAP(ARDUINO_ESP8266_GIT_DESC), EXPECTED_ESP_VERSION)) {
		DEBUG_PRINT("WARNING: Expected Arduino version for this firmware: " EXPECTED_ESP_VERSION "\n");
	}
#endif // DEBUG

	// Sets up configuration HTTP server with soft AP
	WiFi.persistent(true);
	EEPROM.begin(sizeof(struct config_t));
	confServer.onNotFound(handleHTTP);
	confServer.begin();

	// Sets up OTA update server
	ArduinoOTA.begin();

	// Adds mDNS querying support
	MDNS.begin(WiFi.softAPSSID());
	MDNS.addService("http", "tcp", 80);

	// Initializes wifi, ATEM connection, SDI shield and GPIO LEDs
	waccat_init();
}

void loop() {
	MDNS.update();
	confServer.handleClient();
	ArduinoOTA.handle();
}
