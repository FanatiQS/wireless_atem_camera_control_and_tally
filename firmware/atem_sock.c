#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stddef.h> // NULL
#include <stdbool.h> // bool, true, false

#include <lwip/udp.h> // struct udp_pcb, udp_send, udp_new, udp_recv, udp_connect, udp_remove
#include <lwip/pbuf.h> // struct pbuf, pbuf_alloc_reference, PBUF_REF, pbuf_free, pbuf_copy_partial
#include <lwip/ip_addr.h> // ip_addr_t, IPADDR4_INIT, ip_2_ip4
#include <lwip/err.h> // err_t, ERR_OK
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/timeouts.h> // sys_timeout, sys_untimeout
#include <lwip/netif.h> // struct netif, netif_ip4_addr, netif_ip4_netmask, netif_ip4_gw, NETIF_FOREACH, netif_is_up, netif_is_link_up
#include <lwip/ip4.h> // ip4_route
#include <lwip/ip4_addr.h> // ip4_addr_isany_val, ip4_addr_netcmp, ip4_addr_t

#include "../core/atem.h" // struct atem atem_connection_reset, atem_parse, ATEM_STATUS_WRITE, ATEM_STATUS_CLOSING, ATEM_STATUS_REJECTED, ATEM_STATUS_WRITE_ONLY, ATEM_STATUS_CLOSED, ATEM_STATUS_ACCEPTED, ATEM_STATUS_ERROR, ATEM_STATUS_NONE, ATEM_TIMEOUT, ATEM_PORT, atem_cmd_available, atem_cmd_next, ATEM_CMDNAME_VERSION, ATEM_CMDNAME_TALLY, ATEM_CMDNAME_CAMERACONTROL, atem_protocol_major, atem_protocol_minor, ATEM_TIMEOUT_MS
#include "../core/atem_protocol.h" // ATEM_INDEX_FLAGS, ATEM_INDEX_REMOTEID_HIGH, ATEM_INDEX_REMOTEID_LOW, ATEM_FLAG_ACK
#include "./user_config.h" // DEBUG_TALLY, DEBUG_CC, DEBUG_ATEM, PIN_CONN, PIN_PGM, PIN_PVW, PIN_SCL, PIN_SDA
#include "./led.h" // LED_TALLY, LED_CONN, led_init
#include "./sdi.h" // SDI_ENABLED, sdi_write_tally, sdi_write_cc, sdi_init
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, DEBUG_IP, IP_FMT, IP_VALUE, WRAP, DEBUG_ATEM_PRINTF
#include "./wlan.h" // wlan_softap_disable
#include "./ws2812.h" // ws2812_init, ws2812_update
#include "./atem_sock.h"



// Logging string for PGM tally pin
#ifdef PIN_PGM
#define BOOT_INFO_PIN_PGM "Tally PGM pin: " WRAP(PIN_PGM) "\n"
#else // PIN_PGM
#define BOOT_INFO_PIN_PGM "Tally PGM: disabled\n"
#endif // PIN_PGM

// Logging string for PVW tally pin
#ifdef PIN_PVW
#define BOOT_INFO_PIN_PVW "Tally PVW pin: " WRAP(PIN_PVW) "\n"
#else // PIN_PVW
#define BOOT_INFO_PIN_PVW "Tally PVW: disabled\n"
#endif // PIN_PVW

// Logging string for CONN LED pin
#ifdef PIN_CONN
#define BOOT_INFO_PIN_CONN "CONN LED pin: " WRAP(PIN_CONN) "\n"
#else // PIN_CONN
#define BOOT_INFO_PIN_CONN "CONN LED: disabled\n"
#endif // PIN_CONN

// Logging string for RGB tally LED pin
#ifdef PIN_TALLY_RGB
#define BOOT_INFO_PIN_TALLY_RGB "RGB TALLY LED pin: " WRAP(PIN_TALLY_RGB) "\n"
#else // PIN_TALLY_RGB
#define BOOT_INFO_PIN_TALLY_RGB "RGB TALLY LED: disabled\n"
#endif // PIN_TALLY_RGB

// Logging string for I2C pins
#ifdef SDI_ENABLED
#define BOOT_INFO_PIN_I2C "SDI shield I2C SCL pin: " WRAP(PIN_SCL) "\nSDI shield I2C SDA pin: " WRAP(PIN_SDA) "\n"
#else // SDI_ENABLED
#define BOOT_INFO_PIN_I2C "SDI shield: disabled\n"
#endif // SDI_ENABLED



// ATEM connection context
struct atem atem;

// HTML text states of connection to ATEM
const char* atem_state = "Unconnected";
const char* const atem_state_connected = "Connected";
const char* const atem_state_dropped = "Lost connection";
const char* const atem_state_rejected = "Rejected";
const char* const atem_state_disconnected = "Disconnected";



// Resets tally and connection status when disconnected from ATEM
static inline void tally_reset(void) {
	DEBUG_TALLY_PRINTF("Reset\n");
	LED_CONN(false);
	LED_TALLY(false, false);
	ws2812_update(false, false);
	sdi_write_tally(atem.dest, false, false);
	atem.tally_pgm = false;
	atem.tally_pvw = false;
}

// Sends buffered data to ATEM
static void atem_send(struct udp_pcb* pcb) {
	// Creates pbuf container for ATEM data without copying
	struct pbuf* p = pbuf_alloc_reference(atem.write_buf, atem.write_len, PBUF_REF);
	if (p == NULL) {
		DEBUG_ERR_PRINTF("Failed to alloc pbuf for ATEM\n");
		return;
	}

	// Sends data to ATEM and hands over the responsibility of the buffer container to LwIP UDP
	err_t err = udp_send(pcb, p);
	pbuf_free(p);

	// Successfully returns
	if (err == ERR_OK) return;

	// Unsuccessfully returns
	DEBUG_ERR_PRINTF("Failed to send pbuf to ATEM: %d\n", (int)err);
}

// Processes received ATEM packet
static inline void atem_process(struct udp_pcb* pcb) {
	// Parses received ATEM packet
	switch (atem_parse(&atem)) {
		case ATEM_STATUS_ERROR:
		case ATEM_STATUS_CLOSED:
		case ATEM_STATUS_NONE: return;
		case ATEM_STATUS_REJECTED: {
			atem_state = atem_state_rejected;
			DEBUG_PRINTF("ATEM connection rejected\n");
			return;
		}
		case ATEM_STATUS_WRITE: {
			DEBUG_ATEM_PRINTF("packet to acknowledge: %d\n", atem.remote_id_last);
			break;
		}
		case ATEM_STATUS_CLOSING: {
			atem_state = atem_state_disconnected;
			tally_reset();
			DEBUG_PRINTF("ATEM connection closed\n");
			break;
		}
		case ATEM_STATUS_ACCEPTED:
		case ATEM_STATUS_WRITE_ONLY: {
			atem_send(pcb);
#if DEBUG_ATEM
			if (atem.remote_id_last > 0 && atem.write_buf[ATEM_INDEX_FLAGS] == ATEM_FLAG_ACK) {
				DEBUG_ATEM_PRINTF(
					"out-of-order packet: %d\n",
					atem.read_buf[ATEM_INDEX_REMOTEID_HIGH] << 8 |
					atem.read_buf[ATEM_INDEX_REMOTEID_LOW]
				);
			}
#endif // DEBUG_ATEM
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
			wlan_softap_disable();
			break;
		}
		// Outputs tally status on GPIO pins and SDI
		case ATEM_CMDNAME_TALLY: {
			// Only processes tally updates for selected camera
			if (!atem_tally_updated(&atem)) break;

			DEBUG_TALLY_PRINTF(
				"Changed state: %s\n",
				((atem.tally_pgm) ? "PGM" : ((atem.tally_pvw) ? "PVW" : "NONE"))
			);

			// Sets tally pin states
			LED_TALLY(atem.tally_pgm, atem.tally_pvw);

			// Sets RGB tally states
			ws2812_update(atem.tally_pgm, atem.tally_pvw);

			// Writes tally data over SDI
			sdi_write_tally(atem.dest, atem.tally_pgm, atem.tally_pvw);

			break;
		}
#if defined(SDI_ENABLED) || DEBUG_CC
		// Sends camera control data over SDI
		case ATEM_CMDNAME_CAMERACONTROL: {
			// Only processes camera control updates for selected camera
			if (!atem_cc_updated(&atem)) break;

			// Translates ATEM camera control protocol to SDI camera control protocol
			atem_cc_translate(&atem);

#if DEBUG_CC
			char buf_print[255 * 3 + 1];
			uint8_t offset = 0;
			for (uint16_t i = 0; i < atem.cmd_len; i++) {
				buf_print[offset++] = ' ';
				buf_print[offset++] = "0123456789abcdef"[atem.cmd_buf[i] >> 4];
				buf_print[offset++] = "0123456789abcdef"[atem.cmd_buf[i] & 0xf];
			}
			buf_print[offset] = '\0';
			DEBUG_PRINTF("[ Camera Control ] Got camera control data:%s\n", buf_print);
#endif // DEBUG_CC

			// Writes camera control data over SDI
			sdi_write_cc(atem.cmd_buf - 2, atem.cmd_len);

			break;
		}
#endif // SDI_ENABLED || DEBUG_CC
		// Lets compiler know not all command names should be expected
		default: {
			break;
		}
	}
}

// Reconnects ATEM after timeout
static void atem_timeout_callback(void* arg) {
	// Restarts timeout timer
	sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, arg);

	// Sends handshake to ATEM
	atem_connection_reset(&atem);
	atem_send(arg);

	// Indicates connection lost with LEDs, SDI, HTML and serial
	if (atem_state == atem_state_connected) {
		atem_state = atem_state_dropped;
		tally_reset();
		DEBUG_PRINTF("Lost connection to ATEM\n");
	}
	else if (atem_state == atem_state_rejected || atem_state == atem_state_disconnected) {
		DEBUG_PRINTF("Reconnecting to ATEM\n");
	}
	else {
		DEBUG_PRINTF("Failed to connect to ATEM\n");
	}
}

// Reads and processces received ATEM packet
static void atem_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, uint16_t port) {
	// Prevents compiler warnings for unused argument
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(addr);
	LWIP_UNUSED_ARG(port);

	// Copies contents of pbuf to atem structs processing buffer
	if (!pbuf_copy_partial(p, atem.read_buf, sizeof(atem.read_buf), 0)) {
		DEBUG_ERR_PRINTF("Failed to copy ATEM packet\n");
		pbuf_free(p);
		return;
	}

	// Resets drop timeout timer
	sys_untimeout(atem_timeout_callback, pcb);
	sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, pcb);

	// Processes the received ATEM packet
	atem_process(pcb);

	// Releases pbuf
	pbuf_free(p);
}

// Connects to ATEM when network interface is available
static void atem_netif_poll(void* arg) {
	struct netif* netif;
	struct udp_pcb* pcb = arg;
	const ip4_addr_t* addr = ip_2_ip4(&pcb->remote_ip);

	// Waits for netif where ATEM could be found to become available
	NETIF_FOREACH(netif) {
		if (
			netif_is_up(netif)
			&& netif_is_link_up(netif)
			&& !ip4_addr_isany_val(*netif_ip4_addr(netif))
			&& ip4_addr_netcmp(addr, netif_ip4_addr(netif), netif_ip4_netmask(netif))
		) {
			DEBUG_IP(
				"Connected to network interface",
				netif_ip4_addr(netif)->addr,
				netif_ip4_netmask(netif)->addr,
				netif_ip4_gw(netif)->addr
			);
			DEBUG_PRINTF("Connecting to ATEM\n");

			// Sends ATEM handshake
			atem_send(pcb);

			// Enables ATEM timeout callback function
			sys_timeout(ATEM_TIMEOUT_MS, atem_timeout_callback, pcb);

			// Returns when initilization is complete
			return;
		}
	}

	// Keeps waiting until network is connected
	sys_timeout(20, atem_netif_poll, pcb);
}

// Initializes UDP connection to ATEM
struct udp_pcb* atem_init(uint32_t addr, uint8_t dest) {
	// Sets the camera id to serve
	DEBUG_PRINTF("Filtering for camera ID: %d\n", dest);
	atem.dest = dest;

	// Creates pcb (protocol control buffer) for UDP connection
	struct udp_pcb* pcb = udp_new();
	if (pcb == NULL) {
		DEBUG_ERR_PRINTF("Failed to create ATEM UDP pcb\n");
		return NULL;
	}

	// Connects ATEM receive callback function to pcb
	udp_recv(pcb, atem_recv_callback, NULL);

	// Connects to ATEM switcher
	DEBUG_PRINTF("Connecting to ATEM at: " IP_FMT "\n", IP_VALUE(addr));
	err_t err = udp_connect(pcb, &(const ip_addr_t)IPADDR4_INIT(addr), ATEM_PORT);
	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to connect to ATEM UDP IP: %d\n", (int)err);
		udp_remove(pcb);
		return NULL;
	}

	// Prints pin assignments
	DEBUG_PRINTF(
		BOOT_INFO_PIN_PGM
		BOOT_INFO_PIN_PVW
		BOOT_INFO_PIN_CONN
		BOOT_INFO_PIN_TALLY_RGB
		BOOT_INFO_PIN_I2C
	);

	// Starts all status LEDs off
#ifdef PIN_CONN
	led_init(PIN_CONN);
#endif // PIN_CONN
#ifdef PIN_PGM
	led_init(PIN_PGM);
#endif // PIN_PGM
#ifdef PIN_PVW
	led_init(PIN_PVW);
#endif // PIN_PVW
	LED_TALLY(false, false);
	LED_CONN(false);

	// Initializes RGB LED
	ws2812_init();

	// Initializes ATEM struct for handshake
	atem_connection_reset(&atem);

	// Tries to connect to SDI shield
	if (!sdi_init(dest)) {
		udp_remove(pcb);
		return NULL;
	}

	// Starts polling network interface to send ATEM handshake when connected
	sys_timeout(0, atem_netif_poll, pcb);
	return pcb;
}
