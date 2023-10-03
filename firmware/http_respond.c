#include <time.h> // time, NULL, time_t
#include <stdio.h> // sprintf
#include <string.h> // strnlen, strlen
#include <stdint.h> // uint32_t
#include <stddef.h> // size_t
#include <stdbool.h> // bool, false, true

#include <lwip/tcp.h> // tcp_write, TCP_WRITE_FLAG_COPY, tcp_sndbuf, tcp_output, tcp_sndqueuelen
#include <lwip/def.h> // LWIP_MIN
#include <lwip/err.h> // ERR_OK
#include <lwip/ip_addr.h> // ip_addr_t, IPADDR4_INIT, ipaddr_ntoa

#include <user_interface.h> // wifi_station_get_connect_status, STATION_GOT_IP, wifi_station_get_rssi

#include "./debug.h" // DEBUG_PRINTF, DEBUG_HTTP_PRINTF
#include "./http.h" // struct http_t
#include "./init.h" // FIRMWARE_VERSION_STRING
#include "./atem_sock.h" // atem_state, atem
#include "./http_respond.h" // http_respond, http_err, http_post_err
#include "./flash.h" // CACHE_NAME, CACHE_SSID, CACHE_PSK, CONF_FLAG_STATICIP



// Writes as much as TCP PCB can handle of a constant string with known length
static bool http_write(struct http_t* http, char* buf, size_t len) {
	// Shifts buffer pointer if previously partly written
	buf += http->offset;
	len -= http->offset;

	// Writes as much of the buffer as PCB has room for
	size_t size = LWIP_MIN(len, tcp_sndbuf(http->pcb));
	if (tcp_write(http->pcb, buf, size, 0) != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to write TCP data\n");
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
	int len = sprintf(buf, "%uh %02um %02us", (uint32_t)(t / 60 / 60), (uint32_t)(t / 60 % 60), (uint32_t)(t % 60));
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) != ERR_OK;
}

// Writes wifi disconnected status or RSSI if connected to TCP PCB
static inline bool http_write_wifi(struct http_t* http) {
	// Writes status when not connected to network
	if (wifi_station_get_connect_status() != STATION_GOT_IP) {
		return !HTTP_SEND(http, "Not connected");
	}

	// Writes signal strength of network connection
	char buf[sizeof("-128 dBm")];
	int len = sprintf(buf, "%d dBm", wifi_station_get_rssi());
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) != ERR_OK;
}

// Writes HTML input string value with unknown length up to a maximum value to TCP PCB
static inline bool http_write_value_string(struct http_t* http, char* str, size_t maxlen) {
	return !http_write(http, str, strnlen(str, maxlen));
}

// Writes HTML input uint8 value to TCP PCB
static inline bool http_write_value_uint8(struct http_t* http, uint8_t value) {
	char buf[sizeof("255")];
	int len = sprintf(buf, "%u", value);
	return tcp_write(http->pcb, buf, len, TCP_WRITE_FLAG_COPY) != ERR_OK;
}

// Writes HTML input ip address value  to TCP PCB
static inline bool http_write_value_addr(struct http_t* http, uint32_t addr) {
	char* buf = ipaddr_ntoa(&(const ip_addr_t)IPADDR4_INIT(addr));
	return tcp_write(http->pcb, buf, strlen(buf), TCP_WRITE_FLAG_COPY) != ERR_OK;
}



// Creates state machine without having to specify case number for each state
#define HTTP_RESPONSE_CASE(condition) case __LINE__: if (condition) { http->responseState = __LINE__; break; }
#define HTTP_RESPONSE_CASE_STR(http, str) HTTP_RESPONSE_CASE(!HTTP_SEND(http, str))

// Resumable HTTP write state machine handling all responses
bool http_respond(struct http_t* http) {
	switch (http->responseState) {
		// Returns false when all response data has been success transmitted
		case HTTP_RESPONSE_STATE_POST_ROOT_COMPLETE:
		case HTTP_RESPONSE_STATE_NONE: {
			return tcp_sndqueuelen(http->pcb);
		}

		// Writes HTTP response to root GET request
		case HTTP_RESPONSE_STATE_ROOT:
		HTTP_RESPONSE_CASE_STR(http,
			"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
			"<!DOCTYPEhtml>"
				"<meta charset=utf-8>"
				"<meta content=\"width=device-width\"name=viewport>"
				"<title>Configure Device</title>"
				"<style>"
					"body{display:flex;place-content:center}"
					"form{background:#f8f8f8;padding:1em;border-radius:1em}"
					"td{padding:0 1em}"
					"tr:empty{height:1em}"
					"input[maxlength]{width:100%;margin:2px}"
				"</style>"
			"<form method=post><table>"
			"<tr><td>Request time:<td>"
			"<script>document.scripts[0].outerHTML=new Date().toTimeString().slice(0,8)</script>"
			"<tr><td>Time since boot:<td>"
		)
		HTTP_RESPONSE_CASE(http_write_uptime(http))
		HTTP_RESPONSE_CASE_STR(http,
			"<tr>"
			"<tr><td>Firmware Version:<td>" FIRMWARE_VERSION_STRING
			"<tr><td>WiFi signal strength:<td>"
		)
		HTTP_RESPONSE_CASE(http_write_wifi(http))
		HTTP_RESPONSE_CASE_STR(http,
			"<tr><td>ATEM connection status:<td>"
		)
		HTTP_RESPONSE_CASE_STR(http, (char*)atem_state)
		HTTP_RESPONSE_CASE_STR(http,
			"<tr>"
			"<tr><td>Name:<td>"
			"<input required maxlength=32 name=" "name" " value=\""
		)
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_NAME, sizeof(http->cache.CACHE_NAME)))
		HTTP_RESPONSE_CASE_STR(http,
			"\">"
			"<tr>"
			"<tr><td>Network name (SSID):<td>"
			"<input required maxlength=32 name=" "ssid" " value=\""
		)
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_SSID, sizeof(http->cache.CACHE_SSID)))
		HTTP_RESPONSE_CASE_STR(http,
			"\">"
			"<tr><td>Network password (PSK):<td>"
			"<input required maxlength=64 name=" "psk" " value=\""
		)
		HTTP_RESPONSE_CASE(http_write_value_string(http, (char*)http->cache.CACHE_PSK, sizeof(http->cache.CACHE_PSK)))
		HTTP_RESPONSE_CASE_STR(http,
			"\">"
			"<tr>"
			"<tr><td>Camera number:<td>"
			"<input required type=number min=1 max=254 name=" "dest" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_uint8(http, atem.dest))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>ATEM IP:<td>"
			"<input pattern=^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)(\\.(?!$)|$)){4}$ name=" "atem" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.atemAddr))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr>"
			"<tr><td>Use Static IP:<td>"
			"<input type=hidden value=0 name=" "static" ">"
			"<input type=checkbox value=1 name=" "static"
		)
		if (http->cache.config.flags & CONF_FLAG_STATICIP) HTTP_RESPONSE_CASE_STR(http, " checked")
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Local IP:<td>"
			"<input pattern=^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)(\\.(?!$)|$)){4}$ name=" "iplocal" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.localAddr))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Subnet mask:<td>"
			"<input pattern=^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)(\\.(?!$)|$)){4}$ name=" "ipmask" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.netmask))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"<tr><td>Gateway:<td>"
			"<input pattern=^((25[0-5]|(2[0-4]|1\\d|[1-9]|)\\d)(\\.(?!$)|$)){4}$ name=" "ipgw" " value="
		)
		HTTP_RESPONSE_CASE(http_write_value_addr(http, http->cache.config.gateway))
		HTTP_RESPONSE_CASE_STR(http,
			">"
			"</table><button style=\"margin:1em 2em\">Submit</button></form>"
		)
		http->responseState = HTTP_RESPONSE_STATE_NONE;
		break;

		// Writes HTTP error response
		case HTTP_RESPONSE_STATE_ERR:
		HTTP_RESPONSE_CASE_STR(http, "HTTP/1.1 ")
		HTTP_RESPONSE_CASE_STR(http, (char*)http->errCode)
		HTTP_RESPONSE_CASE_STR(http, "\r\nContent-Type: text/plain\r\n\r\n")
		HTTP_RESPONSE_CASE_STR(http, (char*)http->errBody)
		http->responseState = HTTP_RESPONSE_STATE_NONE;
		break;

		// Writes HTTP response to successful POST
		case HTTP_RESPONSE_STATE_POST_ROOT:
		HTTP_RESPONSE_CASE_STR(http, "HTTP/1.1 200 OK\r\n\r\nsuccess")
		http->responseState = HTTP_RESPONSE_STATE_POST_ROOT_COMPLETE;
		break;
	}

	// Transmits all buffered data in TCP PCB from all the writes
	if (tcp_output(http->pcb) != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to output TCP data for %p\n", (void*)http->pcb);
	}
	return true;
}

// Sends response HTTP if entire POST body is parsed
bool http_post_completed(struct http_t* http) {
	if (http->remainingBodyLen > 0) return false;
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
