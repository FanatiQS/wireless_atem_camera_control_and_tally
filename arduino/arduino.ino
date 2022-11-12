/*
 * Configuration:
 * Configuration is done over HTTP by connecting to a web server on the device.
 * A wireless access point is available at boot until the device is connected to
 * an ATEM switcher. If a configuration is submitted, the access point is enabled
 * again to not get locked out if when changing settings. The access point is not
 * enabled when loosing connection to the switcher.
 */

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <DNSServer.h>

#include <lwip/init.h>

#include "atem.h"
#include "html_template_engine.h"
#include "user_config.h"

// Includes libraries required for communicating with the SDI shield if enabled
#ifdef PIN_SCL
#include <Wire.h>
#include <BMDSDIControl.h>
#define USE_SDI // Only for readability
#endif // PIN_SCL



// Firmware version
#define FIRMWARE_VERSION_STRING "0.5.0"

// Expected versions of libraries and SDKs
#define EXPECTED_LWIP_VERSION "2.1.2"
#define EXPECTED_ESP_VERSION "2.7.0"

// Expected SDI shield versions
#define EXPECTED_SDI_LIBRARY_VERSION 1,0
#define EXPECTED_SDI_FIRMWARE_VERSION 0,13
#define EXPECTED_SDI_PROTOCOL_VERSION 1,0



// Throws if only one of SCL and SDA is defined for SDI shield
#if !(defined(PIN_SCL) && defined(PIN_SDA)) && (defined(PIN_SCL) || defined(PIN_SDA))
#error Both PIN_SCL and PIN_SDA has to be defined if SDI shield is to be used or none of them defined if SDI shield is not to be used
#endif

// Throws if tally or camera control debugging is enabled without debugging itself being enabled
#if !DEBUG
#if DEBUG_TALLY
#error To debug tally, enable debugging by defining DEBUG along with DEBUG_TALLY
#endif // DEBUG_TALLY
#if DEBUG_CC
#error To debug camera control, enable debugging by defining DEBUG along with DEBUG_CC
#endif // DEBUG_CC
#endif // !DEBUG



// Only prints debug data when enabled from user_config.h
#if DEBUG
#define DEBUG_PRINT(...) Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else // DEBUG
#define DEBUG_PRINT(arg) do {} while(0)
#define DEBUG_PRINTLN(arg) do {} while(0)
#define DEBUG_PRINTF(...) do {} while(0)
#endif // DEBUG


// Gets the Arduino ESP8266 core version
#define _GET_ESP_VERSION_A(v) #v
#define _GET_ESP_VERSION_B(v) _GET_ESP_VERSION_A(v)
#define ESP_VERSION _GET_ESP_VERSION_B(ARDUINO_ESP8266_GIT_DESC)



// Prints a type of version from the BMDSDIControl library
#if defined(USE_SDI) && DEBUG
void printBMDVersion(char* label, BMD_Version version, uint8_t expectedMajor, uint8_t expectedMinor) {
	// Prints currently used version from the SDI shield
	Serial.print("SDI shield ");
	Serial.print(label);
	Serial.print(" version: ");
	Serial.print(version.Major);
	Serial.print('.');
	Serial.println(version.Minor);

	// Prints warning if current version does not match expected version
	if (expectedMajor != version.Major || expectedMinor != version.Minor) {
		Serial.print("WARNING: Expected SDI shield ");
		Serial.print(label);
		Serial.print(" version: ");
		Serial.print(expectedMajor);
		Serial.print(".");
		Serial.println(expectedMinor);
	}
}
#else // USE_SDI && DEBUG
#define printBMDVersion(lable, version, expectedMajor, expectedMinor) do {} while(0)
#endif // USE_SDI && DEBUG



// Max length for name used in soft AP ssid and mdns lookup
#define NAME_MAX_LEN (16)

// Struct for percistent data used at boot
struct __attribute__((__packed__)) configData_t {
	uint8_t dest;
	uint32_t atemAddr;
	bool useStaticIP;
	uint32_t localAddr;
	uint32_t gateway;
	uint32_t netmask;
	char name[NAME_MAX_LEN];
};

// ATEM communication data that percists between loop cycles
WiFiUDP udp;
IPAddress atemAddr;
struct atem_t atem;
unsigned long timeout = 0;
#ifdef USE_SDI
BMD_SDITallyControl_I2C sdiTallyControl(0x6E);
BMD_SDICameraControl_I2C sdiCameraControl(0x6E);
#endif



// Configuration HTTP server and redirection DNS
ESP8266WebServer confServer(80);
DNSServer dnsServer;

// Names for all HTTP POST keys
#define KEY_SSID "ssid"
#define KEY_PSK "psk"
#define KEY_DEST "dest"
#define KEY_ATEMADDR "atemAddr"
#define KEY_USESTATICIP "useStaticIP"
#define KEY_LOCALIP "localIP"
#define KEY_GATEWAY "gateway"
#define KEY_NETMASK "netmask"
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

// Stores status of connection to ATEM
#define STATUS_UNCONNECTED 0
#define STATUS_CONNECTED 1
#define STATUS_DROPPED 2
#define STATUS_REJECTED 3
#define STATUS_DISCONNECTED 4
uint8_t atemStatus = STATUS_UNCONNECTED;

// Gets string representing status of ATEM connection
#define STATUS_TITLE_UNCONNECTED "Unconnected"
#define STATUS_TITLE_CONNECTED "Connected"
#define STATUS_TITLE_DROPPED "Lost connection"
#define STATUS_TITLE_REJECTED "Rejected"
#define STATUS_TITLE_DISCONNECTED "Disconnected"
#define STATUS_TITLE_UNKNOWN "Unknown"
#define STATUS_LONGEST(str1, str2) ((STRLEN(str1) > STRLEN(str2)) ? str1 : str2)
#define STATUS_LONGEST1 STATUS_LONGEST(STATUS_TITLE_UNCONNECTED, STATUS_TITLE_CONNECTED)
#define STATUS_LONGEST2 STATUS_LONGEST(STATUS_TITLE_DROPPED, STATUS_TITLE_REJECTED)
#define STATUS_LONGEST3 STATUS_LONGEST(STATUS_TITLE_DISCONNECTED, STATUS_TITLE_UNKNOWN)
#define STATUS_LONGEST4 STATUS_LONGEST(STATUS_LONGEST1, STATUS_LONGEST2)
#define STATUS_LONGEST5 (STATUS_LONGEST3)
#define STATUS_LONGEST6 STATUS_LONGEST(STATUS_LONGEST4, STATUS_LONGEST5)
#define STATUS_LEN STRLEN(STATUS_LONGEST6)
char* getAtemStatus() {
	switch (atemStatus) {
		case STATUS_UNCONNECTED: return STATUS_TITLE_UNCONNECTED;
		case STATUS_CONNECTED: return STATUS_TITLE_CONNECTED;
		case STATUS_DROPPED: return STATUS_TITLE_DROPPED;
		case STATUS_REJECTED: return STATUS_TITLE_REJECTED;
		case STATUS_DISCONNECTED: return STATUS_TITLE_DISCONNECTED;
		default: return STATUS_TITLE_UNKNOWN;
	}
}

// HTML configuration page template for use with html templating engine
#define HTML_CONFIG($, conf)\
	HTML($, "<!DOCTYPEhtml>"\
		"<meta content=\"width=device-width\"name=viewport>"\
		"<title>Configure Device</title>"\
		"<style>"\
			"body{display:flex;place-content:center}"\
			"form{background:#f8f8f8;padding:1em;border-radius:1em}"\
			"td{padding:0 1em}"\
			"tr:empty{height:1em}"\
		"</style>"\
		"<form method=post><table>"\
	)\
	HTML_CURRENT_TIME($, "Request time")\
	HTML_TIME($, "Time since boot", time(NULL))\
	HTML_SPACER($)\
	HTML_ROW($, "Firmware Version") HTML($, FIRMWARE_VERSION_STRING)\
	HTML_RSSI($, "WiFi signal strength", WiFi.RSSI(), WiFi.isConnected())\
	HTML_INFO($, "ATEM connection status", getAtemStatus(), STATUS_LEN, "%s", "")\
	HTML_VOLTAGE($, "Voltage level", analogRead(PIN_BATTREAD))\
	HTML_SPACER($)\
	HTML_INPUT_TEXT($, "Name", conf.name, NAME_MAX_LEN, KEY_NAME)\
	HTML_SPACER($)\
	HTML_INPUT_TEXT($, "Network name (SSID)", WiFi.SSID().c_str(), WL_SSID_MAX_LENGTH, KEY_SSID)\
	HTML_INPUT_TEXT($, "Network password (PSK)", WiFi.psk().c_str(), WL_WPA_KEY_MAX_LENGTH, KEY_PSK)\
	HTML_SPACER($)\
	HTML_INPUT_NUMBER($, "Camera number", atem.dest, 254, 1, KEY_DEST)\
	HTML_INPUT_IP($, "ATEM IP", (uint32_t)atemAddr, KEY_ATEMADDR)\
	HTML_SPACER($)\
	HTML_INPUT_CHECKBOX($, "Use Static IP", conf.useStaticIP, KEY_USESTATICIP)\
	HTML_INPUT_IP($, "Local IP", (uint32_t)WiFi.localIP(), KEY_LOCALIP)\
	HTML_INPUT_IP($, "Gateway", (uint32_t)WiFi.gatewayIP(), KEY_GATEWAY)\
	HTML_INPUT_IP($, "Subnet mask", (uint32_t)WiFi.subnetMask(), KEY_NETMASK)\
	HTML($, "</table><button style=\"margin:1em 2em\">Submit</button></form>")

// Gets ip address from 4 HTML post fields as a single 32 bit int
#define IP_FROM_HTTP(server, name) ((server.arg(name "1").toInt()) |\
	(server.arg(name "2").toInt() << 8) | (server.arg(name "3").toInt() << 16) |\
	(server.arg(name "4").toInt() << 24))

// Handles HTTP GET and POST requests for configuration
void handleHTTP() {
	struct configData_t confData;

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
		confData.useStaticIP = confServer.arg(KEY_USESTATICIP).equals("on");
		confData.localAddr = IP_FROM_HTTP(confServer, KEY_LOCALIP);
		confData.gateway = IP_FROM_HTTP(confServer, KEY_GATEWAY);
		confData.netmask = IP_FROM_HTTP(confServer, KEY_NETMASK);
		DEBUG_PRINT("Using static IP: ");
		DEBUG_PRINTLN((confData.useStaticIP) ? "TRUE" : "FALSE");
		DEBUG_PRINT("Local address: ");
		DEBUG_PRINTLN(IPAddress(confData.localAddr));
		DEBUG_PRINT("Gateway: ");
		DEBUG_PRINTLN(IPAddress(confData.gateway));
		DEBUG_PRINT("Subnet mask: ");
		DEBUG_PRINTLN(IPAddress(confData.netmask));

		// Sets device name used for soft ap and mdns
		strncpy(confData.name, confServer.arg(KEY_NAME).c_str(), NAME_MAX_LEN);
		DEBUG_PRINT("Name: ");
		DEBUG_PRINTLN(confData.name);

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
	Serial.begin(9600);

	// Prints mac address that is used as psk by soft AP
	DEBUG_PRINT("Booting...\n");
	DEBUG_PRINT("Mac address: ");
	DEBUG_PRINTLN(WiFi.macAddress());

	// Prints firmware version
	DEBUG_PRINT("Firmware version: " FIRMWARE_VERSION_STRING "\n");

	// Prints libraries and SDKs versions
	DEBUG_PRINT("LWIP version: " LWIP_VERSION_STRING "\n");
	if (strcmp(LWIP_VERSION_STRING, EXPECTED_LWIP_VERSION)) {
		DEBUG_PRINT("WARNING: Expected version for this firmware: " EXPECTED_LWIP_VERSION "\n");
	}
	DEBUG_PRINT("Arduino ESP8266 version: " ESP_VERSION "\n");
	if (strcmp(ESP_VERSION, EXPECTED_ESP_VERSION)) {
		DEBUG_PRINT("WARNING: Expected version for this firmware: " EXPECTED_ESP_VERSION "\n");
	}

	// Prints what debugging flags are enabled when compiled
	DEBUG_PRINTLN(
		"Tally debug: "
#if DEBUG_TALLY
		"enabled"
#else // DEBUG_TALLY
		"disabled"
#endif // DEBUG_TALLY
	);
	DEBUG_PRINTLN(
		"Camera control debug: "
#if DEBUG_CC
		"enabled"
#else // DEBUG_CC
		"disabled"
#endif // DEBUG_CC
	);
#endif // DEBUG



	// Initializes status LED pins
#ifdef PIN_PGM
	pinMode(PIN_PGM, OUTPUT);
	digitalWrite(PIN_PGM, HIGH);
	DEBUG_PRINT("Tally PGM pin: ");
	DEBUG_PRINTLN(PIN_PGM);
#else // PIN_PGM
	DEBUG_PRINT("Tally PGM: disabled\n");
#endif // PIN_PGM
#ifdef PIN_PVW
	pinMode(PIN_PVW, OUTPUT);
	digitalWrite(PIN_PVW, HIGH);
	DEBUG_PRINT("Tally PVW pin: ");
	DEBUG_PRINTLN(PIN_PVW);
#else // PIN_PVW
	DEBUG_PRINT("Tally PVW: disabled\n");
#endif // PIN_PVW
#ifdef PIN_CONN
	pinMode(PIN_CONN, OUTPUT);
	digitalWrite(PIN_CONN, HIGH);
	DEBUG_PRINT("Tally CONN pin: ");
	DEBUG_PRINTLN(PIN_CONN);
#else // PIN_CONN
	DEBUG_PRINT("Tally CONN: disabled\n");
#endif // PIN_CONN



#ifdef USE_SDI
	// Initializes camera control and tally over SDI
	Wire.begin(PIN_SDA, PIN_SCL);
	sdiCameraControl.begin();
	sdiCameraControl.setOverride(true);
	sdiTallyControl.begin();
	sdiTallyControl.setOverride(true);

	// Prints SDI I2C pin assignments
	DEBUG_PRINT("I2C SCL pin: ");
	DEBUG_PRINTLN(PIN_SCL);
	DEBUG_PRINT("I2C SDA pin: ");
	DEBUG_PRINTLN(PIN_SDA);

	// Prints versions used by SDI shield and its library if debugging is enabled
	printBMDVersion("library", sdiCameraControl.getLibraryVersion(), EXPECTED_SDI_LIBRARY_VERSION);
	printBMDVersion("firmware", sdiCameraControl.getFirmwareVersion(), EXPECTED_SDI_FIRMWARE_VERSION);
	printBMDVersion("protocol", sdiCameraControl.getProtocolVersion(), EXPECTED_SDI_PROTOCOL_VERSION);
#else // USE_SDI
	DEBUG_PRINT("SDI shield: disabled\n");
#endif // USE_SDI



	// Gets configuration from persistent memory and updates global conf
	struct configData_t confData;
	EEPROM.begin(sizeof(confData));
	EEPROM.get(0, confData);
	atem.dest = confData.dest;
	atemAddr = confData.atemAddr;

	// Prints configuration data
	DEBUG_PRINT("SSID: ");
	DEBUG_PRINTLN(WiFi.SSID());
	DEBUG_PRINT("PSK: ");
	DEBUG_PRINTLN(WiFi.psk());
	DEBUG_PRINT("Camera ID: ");
	DEBUG_PRINTLN(confData.dest);
	DEBUG_PRINT("ATEM address: ");
	DEBUG_PRINTLN(IPAddress(confData.atemAddr));
#if DEBUG
	if (confData.useStaticIP) {
		DEBUG_PRINT("Using static IP:");
		DEBUG_PRINT("\tLocal address: ");
	DEBUG_PRINTLN(IPAddress(confData.localAddr));
		DEBUG_PRINT("\tGateway: ");
	DEBUG_PRINTLN(IPAddress(confData.gateway));
		DEBUG_PRINT("\tSubnet mask: ");
	DEBUG_PRINTLN(IPAddress(confData.netmask));
	}
	else {
		DEBUG_PRINT("Using DHCP\n");
	}
#endif // DEBUG
	DEBUG_PRINT("Name: ");
	DEBUG_PRINTLN(confData.name);

	// Sets up configuration HTTP server with soft AP
	WiFi.softAP(confData.name, WiFi.macAddress(), 1, 0, 1);
	dnsServer.start(53, "*", WiFi.softAPIP());
	confServer.onNotFound(handleHTTP);
	confServer.begin();
	DEBUG_PRINT("Access point SSID: ");
	DEBUG_PRINTLN(WiFi.softAPSSID());
	DEBUG_PRINT("Access point PSK: ");
	DEBUG_PRINTLN(WiFi.softAPPSK());

	// Connects to last connected WiFi
	WiFi.mode(WIFI_AP_STA);
	WiFi.persistent(true);
	WiFi.setAutoReconnect(true);
	if (confData.useStaticIP) {
		WiFi.config(confData.localAddr, confData.gateway, confData.netmask);
	}
	WiFi.begin();
	udp.begin((RANDOM_REG32 & 0xefff) + 1);
	while (wifi_station_get_connect_status() == STATION_CONNECTING) yield();

	// Adds mDNS querying support
	MDNS.begin(confData.name);
	MDNS.addService("http", "tcp", 80);

	// Prints that setup has completed
	DEBUG_PRINT("Boot complete\n\n");
}

void loop() {
	// Processes configurations over HTTP
	dnsServer.processNextRequest();
	MDNS.update();
	confServer.handleClient();

	// Checks if there is available UDP packet to parse
	if (udp.parsePacket()) {
		// Parses received UDP packet
		udp.read(atem.readBuf, ATEM_MAX_PACKET_LEN);
		switch (atem_parse(&atem)) {
			case ATEM_CONNECTION_OK: break;
			case ATEM_CONNECTION_REJECTED: {
				atemStatus = STATUS_REJECTED;
				DEBUG_PRINT("Rejected by ATEM\n");
				return;
			}
			case ATEM_CONNECTION_CLOSING: {
				atemStatus = STATUS_DISCONNECTED;
				DEBUG_PRINT("Disconnected from ATEM\n");
				return;
			}
			default: return;
		}

		// Processes commands from ATEM
		while (atem_cmd_available(&atem)) switch (atem_cmd_next(&atem)) {
			// Turns on connection status LED and disables access point when connected to ATEM
			case ATEM_CMDNAME_VERSION: {
				DEBUG_PRINT("Connected to ATEM\n");
				DEBUG_PRINT("Got protocol version: ");
				DEBUG_PRINT(atem_protocol_major(&atem));
				DEBUG_PRINT(".");
				DEBUG_PRINTLN(atem_protocol_minor(&atem));
#ifdef PIN_CONN
				digitalWrite(PIN_CONN, LOW);
#endif // PIN_CONN
				WiFi.mode(WIFI_STA);
				atemStatus = STATUS_CONNECTED;
				break;
			}
			// Outputs tally status on GPIO pins and SDI
			case ATEM_CMDNAME_TALLY: {
				if (!atem_tally_updated(&atem)) break;
#if DEBUG_TALLY
				DEBUG_PRINT("Tally state: ");
				if (atem.pgmTally) {
					DEBUG_PRINTLN("PGM");
				}
				else if (atem.pvwTally) {
					DEBUG_PRINTLN("PVW");
				}
				else {
					DEBUG_PRINTLN("NONE");
				}
#endif // DEBUG_TALLY
#ifdef PIN_PGM
				digitalWrite(PIN_PGM, (atem.pgmTally) ? LOW : HIGH);
#endif // PIN_PGM
#ifdef PIN_PVW
				digitalWrite(PIN_PVW, (atem.pvwTally) ? LOW : HIGH);
#endif // PIN_PVW
#ifdef USE_SDI
				sdiTallyControl.setCameraTally(atem.dest, atem.pgmTally, atem.pvwTally);
#endif // USE_SDI
				break;
			}
			// Sends camera control data over SDI
#if defined(USE_SDI) || DEBUG_CC
			case ATEM_CMDNAME_CAMERACONTROL: {
				if (atem_cc_dest(&atem) != atem.dest) break;
				atem_cc_translate(&atem);
#ifdef DEBUG_CC
				DEBUG_PRINT("Got camera control data: ");
				for (uint8_t i = 0; i < atem.cmdLen; i++) {
					DEBUG_PRINT(atem.cmdBuf[i], HEX);
				}
				DEBUG_PRINT("\n");
#endif // DEBUG_CC
#ifdef USE_SDI
				sdiCameraControl.write(atem.cmdBuf, atem.cmdLen);
#endif // USE_SDI
				break;
			}
#endif // USE_SDI || DEBUG_CC
		}
	}
	// Restarts connection to ATEM if it has lost connection
	else {
		if (millis() < timeout) return;
		atem_connection_reset(&atem);
		if (atemStatus == STATUS_CONNECTED) atemStatus = STATUS_DROPPED;
#ifdef PIN_CONN
		digitalWrite(PIN_CONN, HIGH);
#endif // PIN_CONN
#if DEBUG
		if (timeout != 0) {
			DEBUG_PRINT("ATEM status: ");
			DEBUG_PRINTLN(getAtemStatus());
			DEBUG_PRINT("WiFi status: ");
			switch (WiFi.status()) {
				case WL_NO_SHIELD: {
					DEBUG_PRINT("No wifi shield\n");
					break;
				}
				case WL_IDLE_STATUS: {
					DEBUG_PRINT("Idle\n");
					break;
				}
				case WL_NO_SSID_AVAIL: {
					DEBUG_PRINT("No SSID available\n");
					break;
				}
				case WL_SCAN_COMPLETED: {
					DEBUG_PRINT("Scan completed\n");
					break;
				}
				case WL_CONNECTED: {
					DEBUG_PRINT("Connected\n");
					break;
				}
				case WL_CONNECT_FAILED: {
					DEBUG_PRINT("Connect failed\n");
					break;
				}
				case WL_CONNECTION_LOST: {
					DEBUG_PRINT("Connection lost\n");
					break;
				}
#ifdef WL_WRONG_PASSWORD
				case WL_WRONG_PASSWORD: {
					DEBUG_PRINT("Wrong password\n");
					break;
				}
#endif // WL_WRONG_PASSWORD
				case WL_DISCONNECTED: {
					DEBUG_PRINT("Disconnected\n");
					break;
				}
				default: {
					DEBUG_PRINTF("Unknown type (%d)\n", WiFi.status());
					break;
				}
			}
			DEBUG_PRINT("Station SSID: ");
			DEBUG_PRINTLN(WiFi.SSID());
			DEBUG_PRINT("Station PSK: ");
			DEBUG_PRINTLN(WiFi.psk());
			DEBUG_PRINT("Local address: ");
			DEBUG_PRINTLN(WiFi.localIP());
			DEBUG_PRINT("Gateway: ");
			DEBUG_PRINTLN(WiFi.gatewayIP());
			DEBUG_PRINT("Subnet mask: ");
			DEBUG_PRINTLN(WiFi.subnetMask());
			DEBUG_PRINT("\n");
		}
#endif // DEBUG
	}

	// Sends buffered UDP packet to ATEM and resets timeout
	timeout = millis() + ATEM_TIMEOUT * 1000;
	if (!udp.beginPacket(atemAddr, ATEM_PORT)) return;
	udp.write(atem.writeBuf, atem.writeLen);
	udp.endPacket();
}
