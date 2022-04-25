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

// Defines static HTML parts
const char htmlHead[] PROGMEM = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta \
	name=\"viewport\" content=\"width=device-width\"><title>Configure Device\
	</title></head>\<body>";
const char htmlForm1[] PROGMEM = "<form method=\"post\">SSID:<input name=\"ssid\" type=\"text\" value=\"";
const char htmlForm2[] PROGMEM = "\"><br>PSK:<input name=\"psk\" type=\"text\" value=\"";
const char htmlForm3[] PROGMEM = "\"><br>CAMERA ID:<input name=\"camera\" type=\"number\" value=\"";

const char htmlForm4[] PROGMEM = "\"><br>ATEM IP:<input name=\"atem-octet1\" type=\"number\" value=\"";
const char htmlForm5[] PROGMEM = "\"><input name=\"atem-octet2\" type=\"number\" value=\"";
const char htmlForm6[] PROGMEM = "\"><input name=\"atem-octet3\" type=\"number\" value=\"";
const char htmlForm7[] PROGMEM = "\"><input name=\"atem-octet4\" type=\"number\" value=\"";

const char htmlForm8[] PROGMEM = "\"><br>LOCAL IP:<input name=\"local-octet1\" type=\"number\" value=\"";
const char htmlForm9[] PROGMEM = "\"><input name=\"local-octet2\" type=\"number\" value=\"";
const char htmlForm10[] PROGMEM = "\"><input name=\"local-octet3\" type=\"number\" value=\"";
const char htmlForm11[] PROGMEM = "\"><input name=\"local-octet4\" type=\"number\" value=\"";

const char htmlForm12[] PROGMEM = "\"><br>GATEWAY:<input name=\"gateway-octet1\" type=\"number\" value=\"";
const char htmlForm13[] PROGMEM = "\"><input name=\"gateway-octet2\" type=\"number\" value=\"";
const char htmlForm14[] PROGMEM = "\"><input name=\"gateway-octet3\" type=\"number\" value=\"";
const char htmlForm15[] PROGMEM = "\"><input name=\"gateway-octet4\" type=\"number\" value=\"";

const char htmlForm16[] PROGMEM = "\">SUBNETMASK<br><input name=\"subnetmask-octet1\" type=\"number\" value=\"";
const char htmlForm17[] PROGMEM = "\"><input name=\"subnetmask-octet2\" type=\"number\" value=\"";
const char htmlForm18[] PROGMEM = "\"><input name=\"subnetmask-octet3\" type=\"number\" value=\"";
const char htmlForm19[] PROGMEM = "\"><input name=\"subnetmask-octet4\" type=\"number\" value=\"";

const char htmlForm20[] PROGMEM = "\"><br><input type=\"submit\" value=\"Submit\"></form></body</html>";

const char htmlPostRes[] PROGMEM = "<div>Updated, device is reconnecting...</div><div>Reload page \
	when device has restarted, if it is unable to connect to wifi, it will create its own \
	wireless network for configuration</div></body</html>";

// Handles HTTP GET and POST requests for configuration
void handleHTTP() {
	// Processes POST request
	if (confServer.method() == HTTP_POST) {
		// Turn off led to indicate connection is not available anymore
		digitalWrite(LED_BUILTIN, HIGH);

		// Sends response to post before switching network
		confServer.setContentLength(strlen_P(htmlHead) + strlen_P(htmlPostRes));
		confServer.send_P(200, "text/html", "");
		confServer.sendContent_P(htmlHead);
		confServer.sendContent_P(htmlPostRes);
		confServer.client().flush();

		// Sets ssid and psk from POST data
		WiFi.begin(confServer.arg("ssid"), confServer.arg("psk"));
#ifdef DEBUG
		Serial.print("SSID: ");
		Serial.print(WiFi.SSID());
		Serial.print("\nPSK: ");
		Serial.print(WiFi.psk());
		Serial.print("\n");
#endif

		// Sets camera destination
		atem.dest = confServer.arg("camera").toInt();
		EEPROM.write(0, (uint8_t)atem.dest);
#ifdef DEBUG
		Serial.print("Camera ID: ");
		Serial.print(atem.dest);
		Serial.print("\n");
#endif

		// Sets IP address of ATEM
		atemAddr[0] = confServer.arg("atem-octet1").toInt();
		atemAddr[1] = confServer.arg("atem-octet2").toInt();
		atemAddr[2] = confServer.arg("atem-octet3").toInt();
		atemAddr[3] = confServer.arg("atem-octet4").toInt();
		EEPROM.write(1, (uint8_t)atemAddr[0]);
		EEPROM.write(2, (uint8_t)atemAddr[1]);
		EEPROM.write(3, (uint8_t)atemAddr[2]);
		EEPROM.write(4, (uint8_t)atemAddr[3]);
#ifdef DEBUG
		Serial.print("ATEM address: ");
		Serial.print(atemAddr[0]);
		Serial.print(".");
		Serial.print(atemAddr[1]);
		Serial.print(".");
		Serial.print(atemAddr[2]);
		Serial.print(".");
		Serial.print(atemAddr[3]);
		Serial.print("\n");
#endif

		// Sets static local IP address
		localAddr[0] = confServer.arg("local-octet1").toInt();
		localAddr[1] = confServer.arg("local-octet2").toInt();
		localAddr[2] = confServer.arg("local-octet3").toInt();
		localAddr[3] = confServer.arg("local-octet4").toInt();
		EEPROM.write(5, (uint8_t)localAddr[0]);
		EEPROM.write(6, (uint8_t)localAddr[1]);
		EEPROM.write(7, (uint8_t)localAddr[2]);
		EEPROM.write(8, (uint8_t)localAddr[3]);
#ifdef DEBUG
		Serial.print("Local address: ");
		Serial.print(localAddr[0]);
		Serial.print(".");
		Serial.print(localAddr[1]);
		Serial.print(".");
		Serial.print(localAddr[2]);
		Serial.print(".");
		Serial.print(localAddr[3]);
		Serial.print("\n");
#endif

		// Sets static gateway
		gateway[0] = confServer.arg("gateway-octet1").toInt();
		gateway[1] = confServer.arg("gateway-octet2").toInt();
		gateway[2] = confServer.arg("gateway-octet3").toInt();
		gateway[3] = confServer.arg("gateway-octet4").toInt();
		EEPROM.write(9, (uint8_t)gateway[0]);
		EEPROM.write(10, (uint8_t)gateway[1]);
		EEPROM.write(11, (uint8_t)gateway[2]);
		EEPROM.write(12, (uint8_t)gateway[3]);
#ifdef DEBUG
		Serial.print("Gateway: ");
		Serial.print(gateway[0]);
		Serial.print(".");
		Serial.print(gateway[1]);
		Serial.print(".");
		Serial.print(gateway[2]);
		Serial.print(".");
		Serial.print(gateway[3]);
		Serial.print("\n");
#endif

		// Sets IP address
		subnetMask[0] = confServer.arg("subnetmask-octet1").toInt();
		subnetMask[1] = confServer.arg("subnetmask-octet2").toInt();
		subnetMask[2] = confServer.arg("subnetmask-octet3").toInt();
		subnetMask[3] = confServer.arg("subnetmask-octet4").toInt();
		EEPROM.write(13, (uint8_t)subnetMask[0]);
		EEPROM.write(14, (uint8_t)subnetMask[1]);
		EEPROM.write(15, (uint8_t)subnetMask[2]);
		EEPROM.write(16, (uint8_t)subnetMask[3]);
#ifdef DEBUG
		Serial.print("Subnet mask: ");
		Serial.print(subnetMask[0]);
		Serial.print(".");
		Serial.print(subnetMask[1]);
		Serial.print(".");
		Serial.print(subnetMask[2]);
		Serial.print(".");
		Serial.print(subnetMask[3]);
		Serial.print("\n");
#endif

		// Commits percistent data
		EEPROM.commit();

		// Starts access point if new configuration fails and makes the device unreachable
		WiFi.mode(WIFI_AP_STA);

		// Change udp port to ensure no collission between old and new connection
		udp.begin(udp.localPort() + 1);

		// Force reconnecting to atem
		resetAtemState(&atem);
	}
	// Processes GET requests
	else {
		// Gets injection strings
		const String ssid = WiFi.SSID();
		const String psk = WiFi.psk();
		const String destStr(atem.dest);
		const String atemOctet1(atemAddr[0]);
		const String atemOctet2(atemAddr[1]);
		const String atemOctet3(atemAddr[2]);
		const String atemOctet4(atemAddr[3]);
		const String localOctet1(localAddr[0]);
		const String localOctet2(localAddr[1]);
		const String localOctet3(localAddr[2]);
		const String localOctet4(localAddr[3]);
		const String gatewayOctet1(gateway[0]);
		const String gatewayOctet2(gateway[1]);
		const String gatewayOctet3(gateway[2]);
		const String gatewayOctet4(gateway[3]);
		const String subnetmaskOctet1(subnetMask[0]);
		const String subnetmaskOctet2(subnetMask[1]);
		const String subnetmaskOctet3(subnetMask[2]);
		const String subnetmaskOctet4(subnetMask[3]);

		// Sets length of composed HTTP response
		confServer.setContentLength(strlen_P(htmlHead) + strlen_P(htmlForm1) +
			strlen_P(htmlForm2) + strlen_P(htmlForm3) + strlen_P(htmlForm4) +
			strlen_P(htmlForm5) + strlen_P(htmlForm6) + strlen_P(htmlForm7) +
			strlen_P(htmlForm8) + strlen_P(htmlForm9) + strlen_P(htmlForm10) +
			strlen_P(htmlForm11) + strlen_P(htmlForm12) + strlen_P(htmlForm13) +
			strlen_P(htmlForm14) + strlen_P(htmlForm15) + strlen_P(htmlForm16) +
			strlen_P(htmlForm17) + strlen_P(htmlForm18) + strlen_P(htmlForm19) +
			strlen_P(htmlForm20) + ssid.length() + psk.length() + destStr.length() +
			atemOctet1.length() + atemOctet2.length() +
			atemOctet3.length() + atemOctet4.length() +
			localOctet1.length() + localOctet2.length() +
			localOctet3.length() + localOctet4.length() +
			gatewayOctet1.length() + gatewayOctet2.length() +
			gatewayOctet3.length() + gatewayOctet4.length() +
			subnetmaskOctet1.length() + subnetmaskOctet2.length() +
			subnetmaskOctet3.length() + subnetmaskOctet4.length());

		// Sends content as HTTP
		confServer.send(200, "text/html", "");
		confServer.sendContent_P(htmlHead);
		confServer.sendContent_P(htmlForm1);
		confServer.sendContent(ssid);
		confServer.sendContent_P(htmlForm2);
		confServer.sendContent(psk);
		confServer.sendContent_P(htmlForm3);
		confServer.sendContent(destStr);

		confServer.sendContent_P(htmlForm4);
		confServer.sendContent(atemOctet1);
		confServer.sendContent_P(htmlForm5);
		confServer.sendContent(atemOctet2);
		confServer.sendContent_P(htmlForm6);
		confServer.sendContent(atemOctet3);
		confServer.sendContent_P(htmlForm7);
		confServer.sendContent(atemOctet4);

		confServer.sendContent_P(htmlForm8);
		confServer.sendContent(localOctet1);
		confServer.sendContent_P(htmlForm9);
		confServer.sendContent(localOctet2);
		confServer.sendContent_P(htmlForm10);
		confServer.sendContent(localOctet3);
		confServer.sendContent_P(htmlForm11);
		confServer.sendContent(localOctet4);

		confServer.sendContent_P(htmlForm12);
		confServer.sendContent(gatewayOctet1);
		confServer.sendContent_P(htmlForm13);
		confServer.sendContent(gatewayOctet2);
		confServer.sendContent_P(htmlForm14);
		confServer.sendContent(gatewayOctet3);
		confServer.sendContent_P(htmlForm15);
		confServer.sendContent(gatewayOctet4);

		confServer.sendContent_P(htmlForm16);
		confServer.sendContent(subnetmaskOctet1);
		confServer.sendContent_P(htmlForm17);
		confServer.sendContent(subnetmaskOctet2);
		confServer.sendContent_P(htmlForm18);
		confServer.sendContent(subnetmaskOctet3);
		confServer.sendContent_P(htmlForm19);
		confServer.sendContent(subnetmaskOctet4);
		confServer.sendContent_P(htmlForm20);
#ifdef DEBUG

		Serial.print("Sent HTML configuration form to client\n");
#endif
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
