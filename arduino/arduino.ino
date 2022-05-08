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

#include "atem.h"
#include "html_template_engine.h"
#include "user_config.h"

// Both SCL and SDA has to be defined for SDI shield to be enabled
#if defined(PIN_SCL) && defined(PIN_SDA)
#define USE_SDI
#elif defined(PIN_SCL) || defined(PIN_SDA)
#error Both PIN_SCL and PIN_SDA has to be defined if SDI shield is to be used or none of them defined if SDI shield is not to be used
#endif

#ifdef USE_SDI
#include <Wire.h>
#include <BMDSDIControl.h>
#endif

// Throws if tally or camera control debugging is enabled without debugging itself being enabled
#ifndef DEBUG
#ifdef DEBUG_TALLY
#error To debug tally, enable debugging by defining DEBUG along with DEBUG_TALLY
#endif
#ifdef DEBUG_CC
#error To debug camera control, enable debugging by defining DEBUG along with DEBUG_CC
#endif
#endif



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
#define STATUS_LONGEST(str1, str2) ((STRLEN(str1) > STRLEN(str2)) ? str1 : str2)
#define STATUS_LONGEST1 STATUS_LONGEST(STATUS_TITLE_UNCONNECTED, STATUS_TITLE_CONNECTED)
#define STATUS_LONGEST2 STATUS_LONGEST(STATUS_TITLE_DROPPED, STATUS_TITLE_REJECTED)
#define STATUS_LONGEST3 (STATUS_TITLE_DISCONNECTED)
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
		default: return "Unknown";
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
 		confServer.send(200, "text/html", \
			"<!DOCTYPEhtml><meta content=\"width=device-width\"name=viewport>"\
			"<title>Configure Device</title>"\
			"<div>Updated, device is rebooting..."\
			"<div>Reload page when device has restarted"
			"<div>If device is unable to connect to wifi, it will "\
				"create its own wireless network for configuration."
		);
		confServer.client().flush();

		// Sets WiFi ssid and psk
		WiFi.begin(confServer.arg(KEY_SSID), confServer.arg(KEY_PSK));
#ifdef DEBUG
		Serial.print("SSID: ");
		Serial.println(WiFi.SSID());
		Serial.print("PSK: ");
		Serial.println(WiFi.psk());
#endif

		// Sets camera destination and IP address of ATEM
		confData.dest = confServer.arg(KEY_DEST).toInt();
		confData.atemAddr = IP_FROM_HTTP(confServer, KEY_ATEMADDR);
#ifdef DEBUG
		Serial.print("Camera ID: ");
		Serial.println(confData.dest);
		Serial.print("ATEM address: ");
		Serial.println(IPAddress(confData.atemAddr));
#endif

		// Sets static IP data
		confData.useStaticIP = confServer.arg(KEY_USESTATICIP).equals("on");
		confData.localAddr = IP_FROM_HTTP(confServer, KEY_LOCALIP);
		confData.gateway = IP_FROM_HTTP(confServer, KEY_GATEWAY);
		confData.netmask = IP_FROM_HTTP(confServer, KEY_NETMASK);
#ifdef DEBUG
		Serial.print("Using static IP: ");
		Serial.println((confData.useStaticIP) ? "TRUE" : "FALSE");
		Serial.print("Local address: ");
		Serial.println(IPAddress(confData.localAddr));
		Serial.print("Gateway: ");
		Serial.println(IPAddress(confData.gateway));
		Serial.print("Subnet mask: ");
		Serial.println(IPAddress(confData.netmask));
#endif

		// Sets device name used for soft ap and mdns
		strncpy(confData.name, confServer.arg(KEY_NAME).c_str(), NAME_MAX_LEN);
#ifdef DEBUG
		Serial.print("Name: ");
		Serial.println(confData.name);
#endif

		// Writes and commits configuration data to percist for next boot
		EEPROM.put(0, confData);
		EEPROM.commit();

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
#ifdef DEBUG
	Serial.begin(9600);
	Serial.print("Starting...\nMac address: ");
	Serial.println(WiFi.macAddress());
#endif

	// Initializes status LED pins
#ifdef PIN_PGM
	pinMode(PIN_PGM, OUTPUT);
	digitalWrite(PIN_PGM, HIGH);
#endif
#ifdef PIN_PVW
	pinMode(PIN_PVW, OUTPUT);
	digitalWrite(PIN_PVW, HIGH);
#endif
#ifdef PIN_CONN
	pinMode(PIN_CONN, OUTPUT);
	digitalWrite(PIN_CONN, HIGH);
#endif

	// Gets configuration from persistent memory and updates global conf
	struct configData_t confData;
	EEPROM.begin(sizeof(confData));
	EEPROM.get(0, confData);
	atem.dest = confData.dest;
	atemAddr = confData.atemAddr;

	// Initializes camera control and tally over SDI
#ifdef USE_SDI
	Wire.begin(PIN_SDA, PIN_SCL);
	//!! sdiTallyControl.begin();
	//!! sdiTallyControl.setOverride(true);
	//!! sdiCameraControl.begin();
	//!! sdiCameraControl.setOverride(true);
#endif

	// Sets up configuration HTTP server with soft AP
	WiFi.softAP(confData.name, WiFi.macAddress(), 1, 0, 1);
	dnsServer.start(53, "*", WiFi.softAPIP());
	confServer.onNotFound(handleHTTP);
	confServer.begin();

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
		switch (parseAtemData(&atem)) {
			case ATEM_CONNECTION_OK: break;
			case ATEM_CONNECTION_REJECTED: {
				atemStatus = STATUS_REJECTED;
				return;
			}
			case ATEM_CONNECTION_CLOSING: {
				atemStatus = STATUS_DISCONNECTED;
				return;
			}
			default: return;
		}

		// Processes commands from ATEM
		while (hasAtemCommand(&atem)) switch (nextAtemCommand(&atem)) {
			// Turns on connection status LED and disables access point when connected to ATEM
			case ATEM_CMDNAME_VERSION: {
#ifdef DEBUG
				Serial.print("Connected to ATEM\n");
				Serial.print("Got protocol version: ");
				Serial.print(protocolVersionMajor(&atem));
				Serial.print(".");
				Serial.println(protocolVersionMinor(&atem));
#endif
#ifdef PIN_CONN
				digitalWrite(PIN_CONN, LOW);
#endif
				WiFi.mode(WIFI_STA);
				atemStatus = STATUS_CONNECTED;
				break;
			}
			// Outputs tally status on GPIO pins and SDI
			case ATEM_CMDNAME_TALLY: {
				if (tallyHasUpdated(&atem)) {
#ifdef DEBUG_TALLY
					Serial.print("Tally state: ");
					if (atem.pgmTally) {
						Serial.print("PGM");
					}
					else if (atem.pvwTally) {
						Serial.print("PVW");
					}
					else {
						Serial.print("NONE");
					}
					Serial.print("\n");
#endif
#ifdef PIN_PGM
					digitalWrite(PIN_PGM, (atem.pgmTally) ? LOW : HIGH);
#endif
#ifdef PIN_PVW
					digitalWrite(PIN_PVW, (atem.pvwTally) ? LOW : HIGH);
#endif
				}
#ifdef USE_SDI
				translateAtemTally(&atem);
				//!! sdiTallyControl.write(atem.cmdBuf, atem.cmdLen);
#endif
				break;
			}
			// Sends camera control data over SDI
#if defined(DEBUG_CC) || defined(USE_SDI)
			case ATEM_CMDNAME_CAMERACONTROL: {
#ifdef DEBUG_CC
				Serial.print("Got camera control data\n");
#endif
#ifdef USE_SDI
				translateAtemCameraControl(&atem);
				//!! sdiCameraControl.write(atem.cmdBuf);
#endif
				break;
			}
#endif
		}
	}
	// Restarts connection to ATEM if it has lost connection
	else {
		if (millis() < timeout) return;
		resetAtemState(&atem);
		if (atemStatus == STATUS_CONNECTED) atemStatus = STATUS_DROPPED;
#ifdef PIN_CONN
		digitalWrite(PIN_CONN, HIGH);
#endif
#ifdef DEBUG
		if (timeout != 0) {
			Serial.print("No connection to ATEM\n");
		}
#endif
	}

	// Sends buffered UDP packet to ATEM and resets timeout
	timeout = millis() + ATEM_TIMEOUT * 1000;
	if (!udp.beginPacket(atemAddr, ATEM_PORT)) return;
	udp.write(atem.writeBuf, atem.writeLen);
	udp.endPacket();
}
