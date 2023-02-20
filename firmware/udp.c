#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stddef.h> // NULL
#include <stdbool.h> // bool, true, false
#include <string.h> // memcpy // @todo remove when not needed anymore

#include <lwip/udp.h> // struct udp_pcb, udp_send, udp_new, udp_recv, udp_connect, udp_remove, struct pbuf, pbuf_alloc_reference, PBUF_REF, pbuf_free, pbuf_get_contiguous, ip_addr_t, ipaddr_ntoa, err_t, ERR_OK, LWIP_UNUSED_ARG
#include <lwip/timeouts.h> // sys_timeout, sys_untimeout
#include <lwip/init.h> // LWIP_VERSION_STRING
#include <lwip/netif.h> // netif_default
#ifdef ESP8266
#include <user_interface.h> // wifi_set_opmode_current, STATION_MODE
#endif // ESP8266

#include "../src/atem.h" // struct atem_t atem_connection_reset, atem_parse, ATEM_CONNECTION_OK, ATEM_CONNECTION_CLOSING, ATEM_CONNECTION_REJECTED, ATEM_TIMEOUT, ATEM_MAX_PACKET_LEN, ATEM_PORT, atem_cmd_available, atem_cmd_next, ATEM_CMDNAME_VERSION, ATEM_CMDNAME_TALLY, ATEM_CMDNAME_CAMERACONTROL, atem_protocol_majorj, atem_protocol_minor
#include "./user_config.h" // DEBUG_TALLY, DEBUG_CC, PIN_CONN, PIN_PGM, PIN_PVW, PIN_SCL, PIN_SDA
#include "./led.h" // LED_TALLY, LED_CONN, LED_INIT
#include "./sdi.h" // SDI_ENABLED, sdi_write_tally, sdi_write_cc, sdi_connect
#include "./debug.h" // DEBUG_PRINTF
#include "./udp.h"



// Wraps argument into a string
#define _WRAP(arg) #arg
#define WRAP(arg) _WRAP(arg)

// Number of milliseconds before switcher kills the connection for no acknowledge sent
#define ATEM_TIMEOUT_MS (ATEM_TIMEOUT * 1000)

// ATEM connection context
struct atem_t atem;

// HTML text states of connection to ATEM
const char* atem_state;
const char* atem_state_unconnected = "Unconnected";
const char* atem_state_connected = "Connected";
const char* atem_state_dropped = "Lost connection";
const char* atem_state_rejected = "Rejected";
const char* atem_state_disconnected = "Disconnected";



// Sends buffered data to ATEM
static void atem_send(struct udp_pcb* pcb) {
	// Creates pbuf container for ATEM data without copying
	struct pbuf* p = pbuf_alloc_reference(atem.writeBuf, atem.writeLen, PBUF_REF);
	if (p == NULL) {
		DEBUG_PRINTF("Failed to alloc pbuf for ATEM\n");
		return;
	}

	// Sends data to ATEM and releases the buffer container
	err_t err = udp_send(pcb, p);
	pbuf_free(p);

	// Successfully returns
	if (err == ERR_OK) return;

	// Unsuccessfully returns
	DEBUG_PRINTF("Failed to send pbuf to ATEM: %d\n", err);
}

// Processes received ATEM packet
static inline void atem_process(struct udp_pcb* pcb, uint8_t* buf, uint16_t len) {
	memcpy(atem.readBuf, buf, len); // @todo rework atem_parse to not require any copying

	// Parses received ATEM packet
	switch (atem_parse(&atem)) {
		case ATEM_CONNECTION_OK: break;
		case ATEM_CONNECTION_REJECTED: {
			atem_state = atem_state_rejected;
			DEBUG_PRINTF("ATEM connection rejected\n");
			return;
		}
		case ATEM_CONNECTION_CLOSING: {
			atem_state = atem_state_disconnected;
			LED_CONN(false);
			LED_TALLY(false, false);
			atem.pgmTally = false;
			atem.pvwTally = false;
			DEBUG_PRINTF("ATEM connection closed\n");
			return; // @todo should respond with closed packet
		}
		default: {
			DEBUG_PRINTF("Got an error parsing ATEM packet: (%d)\n", atem_parse(&atem));
			return;
		}
	}

	// Sends buffered response to ATEM
	atem_send(pcb);

	// Processes commands received from ATEM
	while (atem_cmd_available(&atem)) switch (atem_cmd_next(&atem)) {
		// Turns on connection status LED and disables access point when connected to ATEM
		case ATEM_CMDNAME_VERSION: {
			DEBUG_PRINTF(
				"Got ATEM protocol version: %d.%d\n"
				"Connected to ATEM\n",
				atem_protocol_major(&atem), atem_protocol_minor(&atem)
			);
			LED_CONN(true);
			atem_state = atem_state_connected;
#ifdef ESP8266
			if (!wifi_set_opmode_current(STATION_MODE)) {
				DEBUG_PRINTF("Failed to disable soft AP\n");
			}
#endif // ESP8266
			break;
		}
		// Outputs tally status on GPIO pins and SDI
		case ATEM_CMDNAME_TALLY: {
			// Only processes tally updates for selected camera
			if (!atem_tally_updated(&atem)) break;

#if DEBUG_TALLY
			DEBUG_PRINTF(
				"Tally state: %s\n",
				((atem.pgmTally) ? "PGM" : ((atem.pvwTally) ? "PVW" : "NONE"))
			);
#endif // DEBUG_TALLY

			// Sets tally pin states
			LED_TALLY(atem.pgmTally, atem.pvwTally);

			// Writes tally data over SDI
			sdi_write_tally(atem.dest, atem.pgmTally, atem.pvwTally);

			break;
		}
#if defined(SDI_ENABLED) || DEBUG_CC
		// Sends camera control data over SDI
		case ATEM_CMDNAME_CAMERACONTROL: {
			// Only processes camera control updates for selected camera
			if (atem_cc_dest(&atem) != atem.dest) break;

			// Translates ATEM camera control protocol to SDI camera control protocol
			atem_cc_translate(&atem);

#if DEBUG_CC
			uint8_t buf[255 * 3 + 1];
			uint8_t offset = 0;
			for (uint16_t i = 0; i < atem.cmdLen; i++) {
				buf[offset++] = ' ';
				buf[offset++] = "0123456789abcdef"[atem.cmdBuf[i] >> 4];
				buf[offset++] = "0123456789abcdef"[atem.cmdBuf[i] & 0xf];
			}
			buf[offset] = '\0';
			DEBUG_PRINTF("Got camera control data:%s\n", buf);
#endif // DEBUG_CC

			// Writes camera control data over SDI
			sdi_write_cc(atem.cmdBuf - 2, atem.cmdLen);

			break;
		}
#endif // SDI_ENABLED || DEBUG_CC
	}
}

// Reconnects ATEM after timeout
static void atem_timeout_callback(void* arg) {
	// Restarts timeout timer
	sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, arg);

	// Sends handshake to ATEM
	atem_connection_reset(&atem);
	atem_send((struct udp_pcb*)arg);

	if (atem_state == atem_state_connected) {
		DEBUG_PRINTF("Lost connection to ATEM\n");
	}
	else if (atem_state == atem_state_dropped || atem_state == atem_state_unconnected) {
		DEBUG_PRINTF("Failed to connect to ATEM\n");
	}

	// Indicates connection lost with LEDs and HTML
	atem_state = atem_state_dropped;
	LED_CONN(false);
	LED_TALLY(false, false);
	atem.pgmTally = false;
	atem.pvwTally = false;
}

// Reads and processces received ATEM packet
static void atem_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, uint16_t port) {
	// Prevents compiler warnings
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(addr);
	LWIP_UNUSED_ARG(port);

	// Reads ATEM packet
	uint8_t _buf[ATEM_MAX_PACKET_LEN];
	uint8_t* buf = pbuf_get_contiguous(p, _buf, ATEM_MAX_PACKET_LEN, p->tot_len, 0);
	if (buf == NULL) {
		DEBUG_PRINTF("Failed to read ATEM packet\n");
		pbuf_free(p);
		return;
	}

	// Resets drop timeout timer
	sys_untimeout(atem_timeout_callback, pcb);
	sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, pcb);

	// Processes the received ATEM packet
	atem_process(pcb, buf, p->tot_len);

	// Releases pbuf
	pbuf_free(p);
}

// Connects to ATEM when network interface is available
static void atem_netif_poll(void* arg) {
	// Keeps waiting until network is connected
	if (netif_default == NULL) {
		sys_timeout(0, atem_netif_poll, arg);
		return;
	}

	DEBUG_PRINTF(
		"Connected to network interface\n"
		"\tIP address: %u.%u.%u.%u\n"
		"\tNetmask: %u.%u.%u.%u\n"
		"\tGateway: %u.%u.%u.%u\n"
		"Connecting to ATEM\n",
		ip4_addr1_16(netif_ip4_addr(netif_default)),
		ip4_addr2_16(netif_ip4_addr(netif_default)),
		ip4_addr3_16(netif_ip4_addr(netif_default)),
		ip4_addr4_16(netif_ip4_addr(netif_default)),
		ip4_addr1_16(netif_ip4_netmask(netif_default)),
		ip4_addr2_16(netif_ip4_netmask(netif_default)),
		ip4_addr3_16(netif_ip4_netmask(netif_default)),
		ip4_addr4_16(netif_ip4_netmask(netif_default)),
		ip4_addr1_16(netif_ip4_gw(netif_default)),
		ip4_addr2_16(netif_ip4_gw(netif_default)),
		ip4_addr3_16(netif_ip4_gw(netif_default)),
		ip4_addr4_16(netif_ip4_gw(netif_default))
	);

	// Sends ATEM handshake
	atem_send((struct udp_pcb*)arg);

	// Enables ATEM timeout callback function
	sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, arg);
}

// Initializes UDP connection to ATEMs
struct udp_pcb* atem_udp_init(uint32_t addr, uint8_t dest) {
	// Sets camera id to serve
	DEBUG_PRINTF("Filtering for camera ID: %d\n", dest);
	atem.dest = dest;

	// Sets initial ATEM connection state
	atem_state = atem_state_unconnected;

	// Creates protocol control buffer for UDP connection
	struct udp_pcb* pcb = udp_new();
	if (pcb == NULL) {
		DEBUG_PRINTF("Failed to create ATEM UDP pcb\n");
		return NULL;
	}

	// Connects ATEM receive callback function to pcb
	udp_recv(pcb, atem_recv_callback, NULL);

	// Connects to ATEM switcher
	DEBUG_PRINTF("Connecting to ATEM at: %s\n", ipaddr_ntoa(&addr));
	if (udp_connect(pcb, &(const ip_addr_t){ .addr = addr }, ATEM_PORT) != ERR_OK) {
		DEBUG_PRINTF("Failed to connect to ATEM UDP IP\n");
		udp_remove(pcb);
		return NULL;
	}

	// Prints pin assignments
	DEBUG_PRINTF(
#ifdef PIN_PGM
		"Tally PGM pin: " WRAP(PIN_PGM) "\n"
#else // PIN_PGM
		"Tally PGM: disabled\n"
#endif //PIN_PGM
#ifdef PIN_PVW
		"Tally PVW pin: " WRAP(PIN_PVW) "\n"
#else // PIN_PVW
		"Tally PVW: disabled\n"
#endif // PIN_PVW
#ifdef WRAP(PIN_CONN)
		"CONN pin: " WRAP(PIN_CONN) "\n"
#else // PIN_CONN
		"CONN: disabled\n"
#endif // PIN_CONN
#ifdef SDI_ENABLED
		"SDI shield I2C SCL pin: " WRAP(PIN_SCL) "\n"
		"SDI shield I2C SDA pin: " WRAP(PIN_SDA) "\n"
#else // SDI_ENABLED
		"SDI shield: disabled\n"
#endif // SDI_ENABLED
	);

	// Starts all status LEDs off
#ifdef PIN_CONN
	LED_INIT(PIN_CONN);
#endif // PIN_CONN
#ifdef PIN_PGM
	LED_INIT(PIN_PGM);
#endif // PIN_PGM
#ifdef PIN_PVW
	LED_INIT(PIN_PVW);
#endif // PIN_PVW
	LED_TALLY(false, false);
	LED_CONN(false);

	// Initializes ATEM struct for handshake
	atem_connection_reset(&atem);

	// Tries to connect to SDI shield
	if (!sdi_init(dest)) {
		udp_remove(pcb);
		return NULL;
	}

	// Starts polling network interface for connected state
	sys_timeout(0, atem_netif_poll, pcb);
	return pcb;
}
