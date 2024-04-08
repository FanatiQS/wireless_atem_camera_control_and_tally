#include <time.h> // time, NULL, time_t
#include <stdio.h> // sprintf
#include <string.h> // strnlen, strlen
#include <stdint.h> // uint32_t
#include <stddef.h> // size_t
#include <stdbool.h> // bool, false, true

#include <lwip/tcp.h> // struct tcp_pcb, tcp_write, TCP_WRITE_FLAG_COPY, tcp_sndbuf, tcp_output, tcp_sndqueuelen, tcp_err, tcp_recv, tcp_poll, tcp_sent, tcp_abort, tcp_shutdown, tcp_recv_fn
#include <lwip/timeouts.h> // sys_timeout
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/def.h> // LWIP_MIN
#include <lwip/err.h> // ERR_OK, ERR_ABRT
#include <lwip/ip_addr.h> // ip_addr_t, IPADDR4_INIT, ipaddr_ntoa
#include <lwip/err.h> // err_t
#include <lwip/pbuf.h> // struct pbuf

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_HTTP_PRINTF
#include "./http.h" // struct http_t
#include "./init.h" // FIRMWARE_VERSION_STRING
#include "./atem_sock.h" // atem_state
#include "./http_respond.h" // http_respond, http_err, http_post_err
#include "./flash.h" // CACHE_NAME, CACHE_SSID, CACHE_PSK, CONF_FLAG_DHCP, flash_cache_write
#include "./wlan.h" // wlan_station_rssi, WLAN_STATION_NOT_CONNECTED



// Writes configuration to flash and restarts device
static void http_reboot_callback_defer(void* arg) {
	flash_cache_write(&((struct http_t*)arg)->cache);
}

// Restarts device on client timeout or successfull close
static err_t http_reboot_callback(void* arg, struct tcp_pcb* pcb) {
	tcp_err(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_poll(pcb, NULL, 0);
	tcp_abort(pcb);
	sys_timeout(0, http_reboot_callback_defer, arg);
	return ERR_ABRT;
}

// Restarts device if client closes successfully
static err_t http_reboot_recv_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
	LWIP_UNUSED_ARG(p);
	LWIP_UNUSED_ARG(err);
	return http_reboot_callback(arg, pcb);
}

// Restarts device async on error to not cause segfault when internally freeing pcb
static void http_reboot_err_callback(void* arg, err_t err) {
	LWIP_UNUSED_ARG(err);
	DEBUG_ERR_PRINTF("HTTP client %p got an error after POST: %d\n", (void*)((struct http_t*)arg)->pcb, (int)err);
	sys_timeout(0, http_reboot_callback_defer, arg);
}

// Enqueues restarting device after TCP pcb has closed
static inline void http_reboot(struct http_t* http) {
	tcp_recv(http->pcb, http_reboot_recv_callback);
	tcp_poll(http->pcb, http_reboot_callback, 2);
	tcp_err(http->pcb, http_reboot_err_callback);
	tcp_sent(http->pcb, NULL);
	tcp_shutdown(http->pcb, false, true);
}



// Writes as much as TCP PCB can handle of a constant string with known length
static bool http_write(struct http_t* http, const char* buf, size_t len) {
	// Shifts buffer pointer if previously partly written
	buf += http->offset;
	len -= http->offset;

	// Writes as much of the buffer as PCB has room for
	size_t size = LWIP_MIN(len, tcp_sndbuf(http->pcb));
	if (tcp_write(http->pcb, buf, size, 0) != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to write HTTP data for %p\n", (void*)http->pcb);
		return false;
	}

	// Sets buffer offset for when writing remaining data
	if (len > size) {
		http->offset += size;
		return false;
	}

	// Buffer has been written in its entirety to PCB
	http->offset = 0;
	return true;
}

// Writes as much as TCP PCB can handle of a constant string automatically getting length
#define HTTP_SEND(http, str) http_write(http, str, strlen(str))



// Writes uptime for device to TCP PCB
static inline bool http_write_uptime(struct http_t* http) {
	char buf[sizeof("1000000h 59m 59s")];
	time_t t = time(NULL);
	int len = sprintf(buf, "%luh %02um %02us", (uint32_t)(t / 60 / 60), (unsigned)(t / 60 % 60), (unsigned)(t % 60));
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) == ERR_OK;
}

// Writes wifi disconnected status or RSSI if connected to TCP PCB
static inline bool http_write_wifi(struct http_t* http) {
	int8_t rssi = wlan_station_rssi();

	// Writes status when not connected to network
	if (rssi == WLAN_STATION_NOT_CONNECTED) {
		return HTTP_SEND(http, "Not connected");
	}

	// Writes signal strength of network connection
	char buf[sizeof("-128 dBm")];
	int len = sprintf(buf, "%d dBm", rssi);
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) == ERR_OK;
}

// Writes HTML input string value with unknown length up to a maximum value to TCP PCB
static bool http_write_value_string(struct http_t* http, char* str, size_t maxlen) {
	maxlen -= http->stringEscapeIndex;
	str += http->stringEscapeIndex;

	size_t len = 0;
	while (len < maxlen && str[len] != '\0') {
		const char* replacement;

		// Escapes quote character
		if (str[len] == '"') {
			replacement = "&#34;";
		}
		// Escapes ampersand character
		else if (str[len] == '&') {
			replacement = "&amp;";
		}
		// Include character in next chunk write
		else {
			len++;
			continue;
		}

		// Writes string up to escaped character
		if (len > 0 && !http_write(http, str, len)) {
			return false;
		}

		// Writes escape sequence
		http->stringEscapeIndex += len;
		if (!HTTP_SEND(http, replacement)) {
			return false;
		}
		str += len + 1;
		maxlen -= len - 1;
		http->stringEscapeIndex += 1;
		len = 0;
	}

	// Writes remaining string
	return http_write(http, str, len);
}

// Writes HTML input uint8 value to TCP PCB
static inline bool http_write_value_uint8(struct http_t* http, uint8_t value) {
	char buf[sizeof("255")];
	int len = sprintf(buf, "%u", value);
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) == ERR_OK;
}

// Writes HTML input ip address value  to TCP PCB
static inline bool http_write_value_addr(struct http_t* http, uint32_t addr) {
	char* buf = ipaddr_ntoa(&(const ip_addr_t)IPADDR4_INIT(addr));
	return tcp_write(http->pcb, buf, strlen(buf), TCP_WRITE_FLAG_COPY) == ERR_OK;
}



// Creates state machine without having to specify case number for each state
#define HTTP_RESPONSE_CASE_BODY(condition) if (!condition) { http->responseState = __LINE__; break; }
#define HTTP_RESPONSE_CASE(condition) /* FALLTHROUGH */ case __LINE__: HTTP_RESPONSE_CASE_BODY(condition)
#define HTTP_RESPONSE_CASE_STR(http, str) HTTP_RESPONSE_CASE(HTTP_SEND(http, str))

// Resumable HTTP write state machine handling all responses
bool http_respond(struct http_t* http) {
	switch (http->responseState) {
		// Returns false when all response data has been success transmitted
		case HTTP_RESPONSE_STATE_NONE: {
			return tcp_sndqueuelen(http->pcb);
		}

		// Writes HTTP response to root GET request
		case HTTP_RESPONSE_STATE_ROOT:
		HTTP_RESPONSE_CASE_STR(http,
			"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
			"<!DOCTYPEhtml>"
				"<meta charset=utf-8>"
				"<meta name=viewport content=width=device-width>"
				"<title>Configure Device</title>"
				"<style>"
					"body{"
						"text-align:center;"
						"font-family:system-ui;"
						"margin:0"
					"}"
					"form{"
						"margin:2em 1em;"
						"display:inline-block;"
						"background:#f8f8f8;"
						"padding:1em;"
						"border-radius:1em;"
						"box-shadow:0 0 20px rgba(0,0,0,.2)"
					"}"
					"td{"
						"text-align:left;"
						"padding:0 .5em;"
						"white-space:nowrap"
					"}"
					"tr{"
						"height:1.2em"
					"}"
					"input:not([type]){"
						"width:150px;"
						"box-sizing:border-box"
					"}"
					"button{"
						"padding:.5em;"
						"background-color:#007bff;"
						"color:#fff;"
						"border:none;"
						"border-radius:4px;"
						"cursor:pointer;"
						"font-size:1em;"
						"margin:2em auto 0;"
						"display:block;"
						"width:80%"
					"}"
					"button:hover{"
						"background-color:#0056b3"
					"}"
					"h1{"
						"margin:.5em"
					"}"
				"</style>"
			"<form method=post>"
			"<h1>WACCAT</h1>"
			"<table>"
			"<tr><td>Firmware Version:<td>" FIRMWARE_VERSION_STRING
			"<tr><td>WiFi signal strength:<td>"
		)
		HTTP_RESPONSE_CASE(http_write_wifi(http))
		HTTP_RESPONSE_CASE_STR(http,
			"<tr><td>ATEM connection status:<td>"
		)
		HTTP_RESPONSE_CASE_STR(http, atem_state)
		HTTP_RESPONSE_CASE_STR(http,
			"<tr><td>Time since boot:<td>"
		)
		HTTP_RESPONSE_CASE(http_write_uptime(http))
		HTTP_RESPONSE_CASE_STR(http,
			"<tr>"
			"<tr><td>Name:<td>"
			"<input maxlength=32 name=" "name" " value=\""
		)
		http->stringEscapeIndex = 0;
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_NAME, sizeof(http->cache.CACHE_NAME)))
		HTTP_RESPONSE_CASE_STR(http,
			"\"required>"
			"<tr>"
			"<tr><td>Network name (SSID):<td>"
			"<input maxlength=32 name=" "ssid" " value=\""
		)
		http->stringEscapeIndex = 0;
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_SSID, sizeof(http->cache.CACHE_SSID)))
		HTTP_RESPONSE_CASE_STR(http,
			"\"required>"
			"<tr><td>Network password (PSK):<td>"
			"<input maxlength=64 name=" "psk" " value=\""
		)
		http->stringEscapeIndex = 0;
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_PSK, sizeof(http->cache.CACHE_PSK)))
		HTTP_RESPONSE_CASE_STR(http,
			"\"required>"
			"<tr>"
			"<tr><td>Camera number:<td>"
			"<input required type=number min=1 max=254 name=" "dest" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_uint8(http, http->cache.config.dest))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>ATEM IP:<td>"
			"<input required pattern=^((25[0-5]|(2[0-4]|1\\d|\\d|)\\d)(\\.(?!$)|$)){4}$ name=" "atem" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.atemAddr))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr>"
			"<tr><td>Use DHCP:<td>"
			"<input type=hidden value=0 name=" "dhcp" ">"
			"<input type=checkbox value=1 name=" "dhcp"
		)
		if (http->cache.config.flags & CONF_FLAG_DHCP) HTTP_RESPONSE_CASE_STR(http, " checked")
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Local IP:<td>"
			"<input required pattern=^((25[0-5]|(2[0-4]|1\\d|\\d|)\\d)(\\.(?!$)|$)){4}$ name=" "localip" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.localAddr))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Subnet mask:<td>"
			"<input required pattern=^((25[0-5]|(2[0-4]|1\\d|\\d|)\\d)(\\.(?!$)|$)){4}$ name=" "netmask" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.netmask))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Gateway:<td>"
			"<input required pattern=^((25[0-5]|(2[0-4]|1\\d|\\d|)\\d)(\\.(?!$)|$)){4}$ name=" "gateway" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.gateway))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"</table><button>Submit</button></form>"
			"<script>"
				"let a=document.querySelector('[type=checkbox][name=dhcp]');"
				"a.onchange=b=>['localip','netmask','gateway']"
					".forEach(c=>document.querySelector(`[name=${c}]`)"
					".disabled=a.checked);a.checked&&a.onchange()"
			"</script>"		
		)
		http->responseState = HTTP_RESPONSE_STATE_NONE;
		break;

		// Writes HTTP error response
		case HTTP_RESPONSE_STATE_ERR:
		HTTP_RESPONSE_CASE_STR(http, "HTTP/1.1 ")
		HTTP_RESPONSE_CASE_STR(http, http->errCode)
		HTTP_RESPONSE_CASE_STR(http, "\r\nContent-Type: text/plain\r\n\r\n")
		HTTP_RESPONSE_CASE_STR(http, http->errBody)
		http->responseState = HTTP_RESPONSE_STATE_NONE;
		break;

		// Writes HTTP response to successful POST and restarts device
		case HTTP_RESPONSE_STATE_POST_ROOT:
		HTTP_RESPONSE_CASE_STR(http,
			"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
			"<!DOCTYPEhtml>"
				"<meta charset=utf-8>"
				"<meta content=\"width=device-width\"name=viewport>"
				"<title>Rebooting...</title>"
				"<style>"
					"body{"
						"text-align:center;"
						"font-family:system-ui;"
						"margin:2em 1em"
					"}"
				"</style>"
			"<p>Device is rebooting...</p>"
			"<p style=font-size:.8em>"
				"Configuration page will automatically reload if device is reachable after reboot."
				"<br>"
				"If device can not connect to an ATEM switcher, "
				"it will create its own wireless network for configuration."
			"</p>"
			"<script>location.reload()</script>"
		)
		http_reboot(http);
		break;
	}

	// Transmits all buffered data in TCP PCB from all the writes
	if (tcp_output(http->pcb) != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to output HTTP data for %p\n", (void*)http->pcb);
	}
	return true;
}

// Sends response HTTP if entire POST body is parsed
bool http_post_completed(struct http_t* http) {
	// Keeps parsing body until received expected length
	if (http->remainingBodyLen > 0) return false;

	// Sends HTTP response
	http->state = HTTP_STATE_DONE;
	http->responseState = HTTP_RESPONSE_STATE_POST_ROOT;
	http->offset = 0;
	http_respond(http);
	return true;
}

// Responds with specified HTTP error code
void http_err(struct http_t* http, const char* code) {
	http->state = HTTP_STATE_DONE;
	http->responseState = HTTP_RESPONSE_STATE_ERR;
	http->offset = 0;
	http->errCode = code;
	http->errBody = code;
	http_respond(http);
}

// Responds with HTTP 400 code and custom message
void http_post_err(struct http_t* http, const char* msg) {
	http->state = HTTP_STATE_DONE;
	http->responseState = HTTP_RESPONSE_STATE_ERR;
	http->offset = 0;
	http->errCode = "400 Bad Request";
	http->errBody = msg;
	http_respond(http);
	DEBUG_HTTP_PRINTF("%s for %p\n", msg, (void*)http->pcb);
}
