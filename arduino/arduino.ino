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

//!! #include <BMDSDIControl.h>

#include "atem.h"
#include "html_template_engine.h"

#define DEBUG



// Struct for percistent data used at boot
struct __attribute__((__packed__)) configData_t {
	uint8_t dest;
	uint32_t atemAddr;
	uint32_t localAddr;
	uint32_t gateway;
	uint32_t netmask;
	bool useStaticIP;
};

// ATEM communication data that percists between loop cycles
WiFiUDP udp;
IPAddress atemAddr;
struct atem_t atem;
unsigned long timeout = 0;
ESP8266WebServer confServer(80);
DNSServer dnsServer;

//!! BMD_SDITallyControl_I2C sdiTallyControl(0x6E);
//!! BMD_SDICameraControl_I2C sdiCameraControl(0x6E);

// HTML configuration page template for use with html templating engine
#define HTML_CONFIG($, conf)\
	HTML($, "<!DOCTYPEhtml><html><head>"\
		"<meta content=\"width=device-width\"name=viewport>"\
		"<title>Configure Device</title>"\
		"<style>"\
			"body{display:flex;place-content:center}"\
			"form{background:#f8f8f8;padding:1em;border-radius:1em}"\
			"td{padding:0 1em}"\
			"tr:empty{height:1em}"\
		"</style>"\
		"</head><body><form method=post><table>"\
	)\
	HTML_INPUT_TEXT($, "Network name (SSID)", WiFi.SSID().c_str(), 32, "ssid")\
	HTML_INPUT_TEXT($, "Network password (PSK)", WiFi.psk().c_str(), 63, "psk")\
	HTML_SPACER($)\
	HTML_INPUT_NUMBER($, "Camera number", atem.dest, 254, 1, "dest")\
	HTML_INPUT_IP($, "ATEM IP", (uint32_t)atemAddr, "atemAddr")\
	HTML_SPACER($)\
	HTML_INPUT_CHECKBOX($, "Use Static IP", conf.useStaticIP, "useStaticIP"\
	HTML_INPUT_IP($, "Local IP", (uint32_t)WiFi.localIP(), "static-localAddr")\
	HTML_INPUT_IP($, "Gateway", (uint32_t)WiFi.gatewayIP(), "static-gateway")\
	HTML_INPUT_IP($, "Subnet mask", (uint32_t)WiFi.subnetMask(), "static-netmask")\
	HTML($, "</table><button style=\"margin:1em 2em\">Submit</button></form></body></html>")

// Gets ip address from 4 HTML post fields as a single 32 bit int
#define IP_FROM_HTTP(server, name) ((server.arg(name "1").toInt()) |\
	(server.arg(name "2").toInt() << 8) | (server.arg(name "3").toInt() << 16) |\
	(server.arg(name "4").toInt() << 24))

// Handles HTTP GET and POST requests for configuration
void handleHTTP() {
	struct configData_t confData;

	// Processes POST request
	if (confServer.method() == HTTP_POST) {
		// Turn off led to indicate connection is not available anymore
		digitalWrite(LED_BUILTIN, HIGH);

		// Sends response to post before switching network
 		confServer.send(200, "text/html", \
			"<!DOCTYPEhtml><meta content=\"width=device-width\"name=viewport>"\
			"<title>Configure Device</title>"\
			"<div>Updated, device is rebooting..."\
			"<div>Reload page when device has restarted, if it is unable to "\
				"connect to wifi, it will create its own "\
				"wireless network for configuration."
		);
		confServer.client().flush();

		// Sets WiFi ssid and psk
		WiFi.begin(confServer.arg("ssid"), confServer.arg("psk"));
#ifdef DEBUG
		Serial.print("SSID: ");
		Serial.println(WiFi.SSID());
		Serial.print("PSK: ");
		Serial.println(WiFi.psk());
#endif

		// Sets camera destination and IP address of ATEM
		confData.dest = confServer.arg("dest").toInt();
		confData.atemAddr = IP_FROM_HTTP(confServer, "atemAddr");
#ifdef DEBUG
		Serial.print("Camera ID: ");
		Serial.println(confData.dest);
		Serial.print("ATEM address: ");
		Serial.println(IPAddress(confData.atemAddr));
#endif

		// Sets static IP data
		confData.useStaticIP = confServer.arg("useStaticIP").equals("on");
		confData.localAddr = IP_FROM_HTTP(confServer, "static-localAddr");
		confData.gateway = IP_FROM_HTTP(confServer, "static-gateway");
		confData.netmask = IP_FROM_HTTP(confServer, "static-netmask");
#ifdef DEBUG
		Serial.print("Using static IP: ");
		Serial.println((confData.useStaticIP) ? "YES" : "NO");
		Serial.print("Local address: ");
		Serial.println(IPAddress(confData.localAddr));
		Serial.print("Gateway: ");
		Serial.println(IPAddress(confData.gateway));
		Serial.print("Subnet mask: ");
		Serial.println(IPAddress(confData.netmask));
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
		char html[HTML_GET_LEN(HTML_CONFIG)];
		sprintf(html, HTML_BUILD(HTML_CONFIG, confData));
		confServer.send(200, "text/html", html);
	}
}



void setup() {
#ifdef DEBUG
	Serial.begin(9600);
#endif

	// Initializes status LED pins
	pinMode(D5, OUTPUT);
	pinMode(D6, OUTPUT);
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(D5, HIGH);
	digitalWrite(D6, HIGH);
	digitalWrite(LED_BUILTIN, HIGH);

	// Gets configuration from persistent memory and updates global conf
	struct configData_t confData;
	EEPROM.begin(sizeof(confData));
	EEPROM.get(0, confData);
	atem.dest = confData.dest;
	atemAddr = confData.atemAddr;

	// Initializes tally over SDI
	//!! sdiTallyControl.begin();
	//!! sdiTallyControl.setOverride(true);

	// Initializes camera control over SDI
	//!! sdiCameraControl.begin();
	//!! sdiCameraControl.setOverride(true);

	// Adds mDNS querying support
	//!! MDNS.begin("esp8266");
	//!! MDNS.addService("http", "tcp", 80);

	// Sets up configuration HTTP server
	WiFi.softAP("Configure_ESP8266", NULL, 1, 0, 1);
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
}

// Stores status of connection to ATEM
#define ATEM_STATUS_OK 0
#define ATEM_STATUS_UNINITIALIZED 0x01
#define ATEM_STATUS_DROPPED 0x02
int8_t atemStatus = ATEM_STATUS_UNINITIALIZED;

void loop() {
	// Processes configurations over HTTP
	dnsServer.processNextRequest();
	//!! MDNS.update();
	confServer.handleClient();

	// Checks if there is available UDP packet to parse
	if (udp.parsePacket()) {
		// Parses received UDP packet
		udp.read(atem.readBuf, ATEM_MAX_PACKET_LEN);
		if (parseAtemData(&atem)) {
			atemStatus = parseAtemData(&atem);
			return;
		}

		// Processes commands from ATEM
		while (hasAtemCommand(&atem)) switch (nextAtemCommand(&atem)) {
			// Turns on builtin LED and disables access point when connected to ATEM
			case ATEM_CMDNAME_VERSION: {
				WiFi.mode(WIFI_STA);
				digitalWrite(LED_BUILTIN, LOW);
				atemStatus = ATEM_STATUS_OK;
				break;
			}
			// Outputs tally status on GPIO pins and SDI
			case ATEM_CMDNAME_TALLY: {
				if (tallyHasUpdated(&atem)) {
#ifdef DEBUG
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
					digitalWrite(D5, (atem.pgmTally) ? LOW : HIGH);
					digitalWrite(D6, (atem.pvwTally) ? LOW : HIGH);
				}
				translateAtemTally(&atem);
				//!! sdiTallyControl.write(atem.cmdBuf, atem.cmdLen);
				break;
			}
			// Sends camera control data over SDI
			case ATEM_CMDNAME_CAMERACONTROL: {
				translateAtemCameraControl(&atem);
				//!! sdiCameraControl.write(atem.cmdBuf);
				break;
			}
		}
	}
	// Restarts connection to ATEM if it has lost connection
	else {
		if (millis() < timeout) return;
		resetAtemState(&atem);
		digitalWrite(LED_BUILTIN, HIGH);
		atemStatus = ATEM_STATUS_DROPPED;
	}

	// Sends buffered UDP packet to ATEM and resets timeout
	timeout = millis() + ATEM_TIMEOUT * 1000;
	if (!udp.beginPacket(atemAddr, ATEM_PORT)) return;
	udp.write(atem.writeBuf, atem.writeLen);
	udp.endPacket();
}
