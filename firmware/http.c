#include <stdbool.h> // bool, true, false
#include <stddef.h> // size_t, NULL
#include <stdint.h> // uint8_t, uint16_t, uint32_t, int32_t, INT32_MAX
#include <string.h> // memcpy
#include <ctype.h> // tolower
#include <string.h> // strlen

#include <lwip/tcp.h> // struct tcp_pcb, tcp_new, tcp_bind, tcp_close, tcp_listen, tcp_accept, tcp_nagle_disable, tcp_poll, tcp_recv, tcp_sent, tcp_err, tcp_recved, tcp_arg, tcp_sndqueuelen, tcp_abort, tcp_write, tcp_output
#include <lwip/err.h> // err_t, ERR_OK, ERR_VAL
#include <lwip/pbuf.h> // struct pbuf, pbuf_free
#include <lwip/ip_addr.h> // IP_ADDR_ANY
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/mem.h> // mem_malloc, mem_free

#include "./user_config.h" // DEBUG_HTTP
#include "./debug.h" // DEBUG_PRINTF, DEBUG_HTTP_PRINTF
#include "./flash.h" // CACHE_SSID, CACHE_PSK, CACHE_NAME, flash_cache_write, flash_cache_read
#include "./http.h" // enum http_state, struct http_t, http_init

// Default port for unencrypted HTTP traffic
#define HTTP_PORT 8080

// The number of TCP coarse grained timer shots before the TCP socket is closed automatically
#define HTTP_POLL_TIMEOUT 10

#define TCP_SEND(pcb, str) tcp_write(pcb, str, (uint16_t)strlen(str), 0)

// Converts percent encoded HEX character set to decimal value
#define HEX_IDLE  (1)
#define HEX_READY (2)
#define HEX_TO_DEC(c) ((c & 0x0f) + 9 * (c >> 6))

// Maximum and minimum camera id (dest) values
#define DEST_MIN (1)
#define DEST_MAX (254)



// Sends buffered HTTP response data
static inline void http_output(struct http_t* http) {
	tcp_output(http->pcb);
	http->state = HTTP_STATE_DONE;
}

// Responds with HTTP status code
// @todo add error handling for TCP_SEND
static void http_err(struct http_t* http, const char* status) {
	TCP_SEND(http->pcb, "HTTP/1.1 ");
	TCP_SEND(http->pcb, status);
	TCP_SEND(http->pcb, "\r\nContent-Type: text/plain\r\n\r\n");
	TCP_SEND(http->pcb, status);
	http_output(http);
}

// Responds with and logs error when parsing HTTP POST body
// @todo add error handling for TCP_SEND
static void http_post_err(struct http_t* http, const char* msg) {
	TCP_SEND(http->pcb, "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\n");
	TCP_SEND(http->pcb, msg);
	http_output(http);
	DEBUG_HTTP_PRINTF("%s for %p\n", msg, (void*)http->pcb);
}



// Checks if HTTP stream has more data available
static bool http_char_available(struct http_t* http) {
	if (http->index == http->p->len) {
		if (http->p->len == http->p->tot_len) return false;
		http->p = http->p->next;
		http->index = 0;
	}
	return true;
}

// Peeks at the next character in the HTTP stream without consuming it
static inline char http_char_peek(struct http_t* http) {
	return ((char*)http->p->payload)[http->index];
}

// Consumes the next character from the HTTP stream
static char http_char_consume(struct http_t* http) {
	char c = http_char_peek(http);
	http->index++;
	return c;
}



// Moves to the next character in the comparison and checks if it has reached its terminator
static inline bool http_cmp_success(struct http_t* http, const char* cmp) {
	http->offset++;
	if (cmp[http->offset] != '\0') {
		return false;
	}
	http->offset = 0;
	return true;
}

// Compares up to the current point from previously half-matched string
static bool http_cmp_back(struct http_t* http, const char* cmp) {
	if (http->cmp == NULL) return true;
	for (uint16_t i = 0; i < http->offset; i++) {
		if (cmp[i] != http->cmp[i]) {
			return false;
		}
	}
	http->cmp = NULL;
	return true;
}

// Compares HTTP stream against substring
static bool http_cmp(struct http_t* http, const char* cmp) {
	if (!http_cmp_back(http, cmp)) return false;
	while (http_char_available(http)) {
		if (http_char_peek(http) != cmp[http->offset]) {
			http->cmp = cmp;
			return false;
		}
		http->index++;
		if (http_cmp_success(http, cmp)) return true;
	}
	return false;
}

// Compares HTTP stream against substring converting stream to lower case
static bool http_cmp_case_insensitive(struct http_t* http, const char* cmp) {
	if (!http_cmp_back(http, cmp)) return false;
	while (http_char_available(http)) {
		if (tolower((int)http_char_peek(http)) != cmp[http->offset]) {
			http->cmp = cmp;
			return false;
		}
		http->index++;
		if (http_cmp_success(http, cmp)) return true;
	}
	return false;
}

// Checks if the last failing comparison failed due to running out of data in the HTTP stream
static inline bool http_cmp_incomplete(struct http_t* http) {
	return (http->cmp == NULL);
}

// Consumes characters up to next occurance of a substring in HTTP stream
static bool http_find(struct http_t* http, const char* cmp) {
	while (http_char_available(http)) {
		if (http_char_consume(http) != cmp[http->offset]) {
			http->offset = 0;
		}
		else if (http_cmp_success(http, cmp)) {
			return true;
		}
	}

	return false;
}



// Compares HTTP stream against POST body key
static bool http_post_key(struct http_t* http, const char* key) {
	if (!http_cmp(http, key)) return false;
	http->remainingBodyLen -= strlen(key);
	DEBUG_HTTP_PRINTF("Found POST body key '%s'\n", key);
	return true;
}

// Comares HTTP stream against POST body key leading to a string value
static bool http_post_key_string(struct http_t* http, const char* key) {
	if (!http_post_key(http, key)) return false;
	http->hex = HEX_IDLE;
	return true;
}

// Checks if the last failing POST body key comparison failed due to running out of data in the HTTP stream
static bool http_post_key_incomplete(struct http_t* http) {
	if (!http_cmp_incomplete(http)) return false;
	return (http->remainingBodyLen > http->offset);
}



static err_t http_recv_reboot_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err);

// Sends response HTTP if entire POST body is parsed
static bool http_post_completed(struct http_t* http) {
	if (http->remainingBodyLen > 0) return false;

	// Writes cache to flash and reboots when close is acknowledged
	tcp_recv(http->pcb, http_recv_reboot_callback);

	// Sends HTTP response to client
	// @todo make better response
	TCP_SEND(http->pcb, "HTTP/1.1 200 OK\r\n\r\nsuccess");
	http_output(http);
	return true;
}



// Completes integer parsing on terminator character
static inline bool http_post_int_complete(struct http_t* http, const char c) {
	if (c != '&') return false;
	http->remainingBodyLen--;
	http->state = HTTP_STATE_POST_ROOT_BODY_KEYS;
	return true;
}

// Validates that the character is a numeric character
static inline bool http_post_int_isnum(const char c) {
	return ((uint8_t)(c - '0') < 10);
}

// Increments integer with numeric character from HTTP stream
static inline bool http_post_octet(struct http_t* http, const char c, uint8_t* addr) {
	uint8_t num = (uint8_t)c - '0';

	// Validates that incrementation will not overflow integer
	if ((*addr + num) > ((UINT8_MAX / 10) + (UINT8_MAX % 10))) return false;

	// Appends the number character to the integer pointer
	*addr *= 10;
	*addr += num;
	http->remainingBodyLen--;

	return true;
}



// Writes POST body string to HTTP clients cache
static bool http_post_value_string(struct http_t* http, char* addr, size_t addrSize) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		http->remainingBodyLen--;

		// Processes hex encoded bytes
		if (http->hex != HEX_IDLE) {
			if (http->hex == HEX_READY) {
				http->hex = HEX_TO_DEC(c) << 4;
				continue;
			}
			else {
				c = (char)http->hex | HEX_TO_DEC(c);
				http->hex = HEX_IDLE;
			}
		}
		// Stops copying string on terminator character
		else if (c == '&') {
			if (http->offset < addrSize) addr[http->offset] = '\0';
			http->hex = 0;
			http->offset = 0;
			http->state = HTTP_STATE_POST_ROOT_BODY_KEYS;
			return true;
		}
		// Translates + sign to space
		else if (c == '+') {
			c = ' ';
		}
		// Activates hex parsing mode
		else if (c == '%') {
			http->hex = HEX_READY;
			continue;
		}

		// Rejects values longer than cache length
		if (http->offset >= addrSize) {
			http_post_err(http, "String POST body value too long");
			return false;
		}

		// Copies over streamed character to cache
		addr[http->offset] = c;
		http->offset++;
	}

	// Stops copying string when entire body is processed
	if (http->offset < addrSize) addr[http->offset] = '\0';
	http_post_completed(http);
	return false;
}

// Writes POST body integer to HTTP clients cache
static bool http_post_value_uint8(struct http_t* http, uint8_t* addr, size_t min, size_t max) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		if (!http_post_int_isnum(c)) {
			if (http_post_int_complete(http, c)) return true;
			http_post_err(http, "Invalid character in integer POST body value");
			return false;
		}
		if (!http_post_octet(http, c, addr) || *addr < min || *addr > max) {
			http_post_err(http, "Integer POST body value out of range");
			return false;
		}
	}
	http_post_completed(http);
	return false;
}

// Writes POST body IPV4 address to HTTP clients cache
static bool http_post_value_ip(struct http_t* http, uint32_t* addr) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		if (http_post_int_isnum(c)) {
			if (!http_post_octet(http, c, ((uint8_t*)addr) + http->offset)) {
				http_post_err(http, "Invalid IPV4 address");
				return false;
			}
		}
		else if (c == '.') {
			// Rejects IP address with more than 4 segments
			if (http->offset >= 3) {
				http_post_err(http, "Invalid IPV4 address");
				return false;
			}

			// Moves to the next IPV4 segment
			http->offset++;
			http->remainingBodyLen--;
		}
		else {
			if (http_post_int_complete(http, c)) {
				http->offset = 0;
				return true;
			}
			http_post_err(http, "Invalid character in IPV4 POST body value");
			return false;
		}
	}
	http_post_completed(http);
	return false;
}

// Writes POST body flag to HTTP clients cache
static bool http_post_value_flag(struct http_t* http, uint8_t* flags, int mask) {
	if (!http_char_available(http)) {
		http_post_completed(http);
		return false;
	}

	switch (http->offset) {
		case 0: {
			char c = http_char_peek(http);
			if (c == '0') {
				*flags &= ~mask;
			}
			else if (c == '1') {
				*flags |= mask;
			}
			else {
				break;
			}
			http->index++;
			http->remainingBodyLen--;
			if (!http_char_available(http)) {
				if (http_post_completed(http)) return false;
				http->offset = 1;
				return true;
			}
			break;
		}
		default: {
			http->offset = 0;
		}
	}

	http->remainingBodyLen--;
	if (http_post_int_complete(http, http_char_consume(http))) return true;
	http_post_err(http, "Invalid character in boolean POST body value");
	return false;
}



/*
 * The function `http_parse` works as a big jump table where each case represents a state.
 * A state is a parsing step that has to be able to be resumed if the HTTP stream runs out of data.
 * When a state has completed its parsing, it can either, based on the parsing result:
 *     fall through: To continue parsing the next state followed in the switch case.
 *     continue (or break): To jump to manually set state that is somewhere else in the control flow.
 *     return: If there is no more data to parse or response is sent.
 */
static inline void http_parse(struct http_t* http, struct pbuf* p) {
	http->p = p;
	http->index = 0;

#ifdef DEBUG_HTTP
	DEBUG_HTTP_PRINTF("Received data from client %p:\n", (void*)http->pcb);
	DEBUG_PRINTF("========START========\n");
	while (true) {
		DEBUG_PRINTF("%.*s\n", p->len, p->payload);
		if (p->len == p->tot_len) break;
		p = p->next;
	}
	DEBUG_PRINTF("=========END=========\n");
#endif // DEBUG_HTTP

	// HTTP streaming parser
	while (true) switch (http->state) {

// =====================================
// HTTP GET request hander chain
// =====================================

		// Parses HTTP GET method
		case HTTP_STATE_METHOD_GET: {
			if (!http_cmp(http, "GET /")) {
				if (http_cmp_incomplete(http)) return;
				http->state = HTTP_STATE_METHOD_POST;
				continue;
			}
			DEBUG_HTTP_PRINTF("Got a GET request from %p\n", (void*)http->pcb);
		}
		// Reject all get requests until a proper HTML tramsmission is implemented
		case HTTP_STATE_GET_404: {
			http_err(http, "404 Not Found");
			DEBUG_HTTP_PRINTF("Invalid GET URI from %p\n", (void*)http->pcb);
			return;
		}

// =====================================
// HTTP POST request handler chain
// =====================================

		// Parses HTTP POST method
		case HTTP_STATE_METHOD_POST: {
			if (!http_cmp(http, "POST /")) {
				if (http_cmp_incomplete(http)) {
					http->state = HTTP_STATE_METHOD_POST;
				}
				else {
					http_err(http, "405 Method Not Allowed");
					DEBUG_HTTP_PRINTF("Invalid HTTP method from %p\n", (void*)http->pcb);
				}
				return;
			}
			DEBUG_HTTP_PRINTF("Got a POST request from %p\n", (void*)http->pcb);
		}
		// Flushes start line up to beginning of next header field
		case HTTP_STATE_POST_ROOT_HEADER_NEXT: {
			if (!http_find(http, "\r\n")) {
				http->state = HTTP_STATE_POST_ROOT_HEADER_NEXT;
				return;
			}
		}
		// Rejects client if content length header is missing from POST request
		case HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_MISSING: {
			if (http_cmp(http, "\r\n")) {
				http_err(http, "411 Length Required");
				DEBUG_HTTP_PRINTF("Missing content length header from %p\n", (void*)http->pcb);
				return;
			}
			if (http_cmp_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_MISSING;
				return;
			}
		}
		// Searches stream for HTTP content length header
		case HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_KEY: {
			if (!http_cmp_case_insensitive(http, "content-length:")) {
				// Continues parsing here when getting more streamed HTTP data
				if (http_cmp_incomplete(http)) {
					http->state = HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_KEY;
					return;
				}

				// Jumps to beginning of next header row on failed header key comparison
				http->state = HTTP_STATE_POST_ROOT_HEADER_NEXT;
				continue;
			}
			http->remainingBodyLen = 0;
		}
		// Gets content length value from HTTP stream
		case HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE: {
			while (true) {
				if (!http_char_available(http)) {
					http->state = HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE;
					return;
				}
				char c = http_char_peek(http);
				uint8_t num = (uint8_t)c - '0';
				if (num < 10) {
					if ((http->remainingBodyLen + num) > ((INT32_MAX / 10) + (INT32_MAX % 10))) {
						http_err(http, "413 Payload Too Large");
						DEBUG_HTTP_PRINTF("Content length too large from %p\n", (void*)http->pcb);
						return;
					}
					http->remainingBodyLen *= 10;
					http->remainingBodyLen += num;
				}
				else if (c != ' ') {
					break;
				}
				http->index++;
			}
		}
		// Rejects client if content length value has non numeric characers before line termination
		case HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_CRLF: {
			if (!http_cmp(http, "\r\n")) {
				if (http_cmp_incomplete(http)) {
					http->state = HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_CRLF;
					return;
				}
				http_err(http, "400 Bad Request");
				DEBUG_HTTP_PRINTF("Invalid characters in content length from %p\n", (void*)http->pcb);
				return;
			}
			http->offset = 2; // Just encountered a CRLF, next scans for 2 in a row
		}
		// Flushes remaining headers to parse body
		case HTTP_STATE_POST_ROOT_HEADER_FLUSH: {
			if (!http_find(http, "\r\n\r\n")) {
				http->state = HTTP_STATE_POST_ROOT_HEADER_FLUSH;
				return;
			}
			DEBUG_HTTP_PRINTF(
				"Got a content length of %u bytes from %p\n",
				http->remainingBodyLen, (void*)http->pcb
			);
		}
		// Reads current configuration into clients cache
		case HTTP_STATE_POST_ROOT_BODY_PREPARE: {
			if (!flash_cache_read(&(http->cache))) {
				http_err(http, "500 Internal Server Error");
				return;
			}
		}
		// Parses next HTTP body key
		case HTTP_STATE_POST_ROOT_BODY_KEYS:
		// Checks if the next POST body key is for wlan ssid
		case HTTP_STATE_POST_ROOT_BODY_SSID_KEY: {
			if (http_post_key_string(http, "ssid=")) {
				http->state = HTTP_STATE_POST_ROOT_BODY_SSID_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_SSID_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for wlan psk
		case HTTP_STATE_POST_ROOT_BODY_PSK_KEY: {
			if (http_post_key_string(http, "psk=")) {
				http->state = HTTP_STATE_POST_ROOT_BODY_PSK_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_PSK_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for device name
		case HTTP_STATE_POST_ROOT_BODY_NAME_KEY: {
			if (http_post_key_string(http, "name=")) {
				http->state = HTTP_STATE_POST_ROOT_BODY_NAME_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_NAME_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for ATEM camera id (dest)
		case HTTP_STATE_POST_ROOT_BODY_DEST_KEY: {
			if (http_post_key(http, "dest=")) {
				http->cache.config.dest = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_DEST_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_DEST_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for ATEM ip address
		case HTTP_STATE_POST_ROOT_BODY_ATEM_KEY: {
			if (http_post_key(http, "atem=")) {
				http->cache.config.atemAddr = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_ATEM_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_ATEM_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip or DHCP
		case HTTP_STATE_POST_ROOT_BODY_IPSTATIC_KEY: {
			if (http_post_key(http, "static=")) {
				http->state = HTTP_STATE_POST_ROOT_BODY_IPSTATIC_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_IPSTATIC_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip local address
		case HTTP_STATE_POST_ROOT_BODY_IPLOCAL_KEY: {
			if (http_post_key(http, "iplocal=")) {
				http->cache.config.localAddr = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_IPLOCAL_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_IPLOCAL_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip netmask
		case HTTP_STATE_POST_ROOT_BODY_IPMASK_KEY: {
			if (http_post_key(http, "ipmask=")) {
				http->cache.config.netmask = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_IPMASK_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_IPMASK_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip gateway
		case HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_KEY: {
			if (http_post_key(http, "ipgw=")) {
				http->cache.config.gateway = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_KEY;
				return;
			}
		}
		// Rejects client on invalid POST body key
		case HTTP_STATE_POST_ROOT_BODY_FLUSH: {
			if (!http_post_completed(http)) {
				http_post_err(http, "Invalid POST body key");
			}
			return;
		}

// =====================================
// HTTP form body value handlers
// =====================================

		// Transfers wlan ssid from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_SSID_VALUE: {
			if (http_post_value_string(http, (char*)http->cache.CACHE_SSID, sizeof(http->cache.CACHE_SSID))) {
				continue;
			}
			return;
		}
		// Transfers wlan psk from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_PSK_VALUE: {
			if (http_post_value_string(http, (char*)http->cache.CACHE_PSK, sizeof(http->cache.CACHE_PSK))) {
				continue;
			}
			return;
		}
		// Transfers device name from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_NAME_VALUE: {
			if (http_post_value_string(http, (char*)http->cache.CACHE_NAME, sizeof(http->cache.CACHE_NAME))) {
				continue;
			}
			return;
		}
		// Transfers camera id (dest) from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_DEST_VALUE: {
			if (http_post_value_uint8(http, &(http->cache.config.dest), DEST_MIN, DEST_MAX)) {
				continue;
			}
			return;
		}
		// Transfers ATEM address from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_ATEM_VALUE: {
			if (http_post_value_ip(http, &(http->cache.config.atemAddr))) {
				continue;
			}
			return;
		}
		// Transfers static ip flag from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_IPSTATIC_VALUE: {
			if (http_post_value_flag(http, &(http->cache.config.flags), CONF_FLAG_STATICIP)) {
				continue;
			}
			return;
		}
		// Transfers static ip local address from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_IPLOCAL_VALUE: {
			if (http_post_value_ip(http, &(http->cache.config.localAddr))) {
				continue;
			}
			return;
		}
		// Transfers static ip netmask from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_IPMASK_VALUE: {
			if (http_post_value_ip(http, &(http->cache.config.netmask))) {
				continue;
			}
			return;
		}
		// Transfers static ip gateway from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_VALUE: {
			if (http_post_value_ip(http, &(http->cache.config.gateway))) {
				continue;
			}
			return;
		}
		// Ignores segment from client that has already sent response to request
		case HTTP_STATE_DONE: return;
	}
}



// Closes HTTP connection and prevents dispaching invalid events after close
static inline err_t http_close(struct tcp_pcb* pcb) {
	err_t err = tcp_close(pcb);
	if (err == ERR_OK) return ERR_OK;
	DEBUG_PRINTF("Failed to close HTTP TCP pcb %p: %d", (void*)pcb, (int)err);
	return err;
}

// Writes cache to flash and reboots
static err_t http_recv_reboot_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
	if (p != NULL) {
		if (err == ERR_OK || err == ERR_MEM) pbuf_free(p);
		return err;
	}
	if (err != ERR_OK) {
		return err;
	}

	struct http_t* http = (struct http_t*)arg;
	DEBUG_HTTP_PRINTF("Closed client %p\n", (void*)pcb);
	err_t ret = http_close(pcb);
	tcp_err(pcb, NULL);
	flash_cache_write(&(http->cache));
	mem_free(arg);
	return ret;
}

// Closes HTTP connection when all response data is sent
static err_t http_sent_callback(void* arg, struct tcp_pcb* pcb, uint16_t len) {
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(len);

	DEBUG_HTTP_PRINTF("Sent %d bytes of data to client %p\n", len, (void*)pcb);

	// Only closes connection when response is completely sent
	if (tcp_sndqueuelen(pcb) != 0) return ERR_OK;

	DEBUG_HTTP_PRINTF("Closing client %p\n", (void*)pcb);
	tcp_poll(pcb, NULL, 0);
	return http_close(pcb);
}

// Processes the received TCP packet data
static err_t http_recv_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
	if (err != ERR_OK) {
		if (p != NULL && err == ERR_MEM) pbuf_free(p);
		return err;
	}

	// Closes the TCP connection on client request
	if (p == NULL) {
		DEBUG_HTTP_PRINTF("Closed client %p\n", (void*)pcb);
		err_t ret = http_close(pcb);
		if (ret != ERR_OK) return ret;
		tcp_err(pcb, NULL);
		mem_free(arg);
		return ERR_OK;
	}

	// Processes the TCP packet data
	http_parse((struct http_t*)arg, p);
	tcp_recved(pcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}

// Frees argument memory on TCP socket error
static void http_err_callback(void* arg, err_t err) {
	LWIP_UNUSED_ARG(err);
	DEBUG_PRINTF("HTTP client %p got an error: %d\n", (void*)((struct http_t*)arg)->pcb, (int)err);
	mem_free(arg);
}

// Closes TCP connection after timeout
static err_t http_drop_callback(void* arg, struct tcp_pcb* pcb) {
	LWIP_UNUSED_ARG(arg);
	DEBUG_HTTP_PRINTF("Dropping client %p due to inactivity\n", (void*)pcb);
	tcp_poll(pcb, NULL, 0);
	return http_close(pcb);
}

// Accepts TCP connection for HTTP configuration server
static err_t http_accept_callback(void* arg, struct tcp_pcb* newpcb, err_t err) {
	LWIP_UNUSED_ARG(arg);

	if (err != ERR_OK) {
		DEBUG_PRINTF("HTTP server received accept error: %d\n", (int)err);
		return ERR_VAL;
	}
	if (newpcb == NULL) {
		DEBUG_PRINTF("HTTP server received undefined pcb\n");
		return ERR_VAL;
	}

	// Creates context to use when parsing HTTP data stream
	struct http_t* http = (struct http_t*)mem_malloc(sizeof(struct http_t));
	if (http == NULL) {
		DEBUG_PRINTF("Failed to allocate http struct new client\n");
		tcp_abort(newpcb);
		return ERR_MEM;
	}
	tcp_arg(newpcb, http);
	http->offset = 0;
	http->state = 0;
	http->pcb = newpcb;
	http->cmp = NULL;

	DEBUG_HTTP_PRINTF("A client connected to configuration server with id %p\n", (void*)newpcb);

	// Sets callback functions for pcb
	tcp_recv(newpcb, http_recv_callback);
	tcp_sent(newpcb, http_sent_callback);
	tcp_err(newpcb, http_err_callback);
	tcp_poll(newpcb, http_drop_callback, HTTP_POLL_TIMEOUT);

	// Disables Nagle's algorithm for faster transmission
	tcp_nagle_disable(newpcb);

	return ERR_OK;
}

// Initializes HTTP configuration server
bool http_init(void) {
	// Creates protocol control buffer for HTTP servers TCP listener
	struct tcp_pcb* pcb = tcp_new();
	if (pcb == NULL) {
		DEBUG_PRINTF("Failed to create HTTP pcb\n");
		return false;
	}

	// Binds TCP listener to allow traffic from any address to a specified port
	err_t err = tcp_bind(pcb, IP_ADDR_ANY, HTTP_PORT);
	if (err != ERR_OK) {
		DEBUG_PRINTF("Failed to bind HTTP listen pcb: %d\n", (int)err);
		tcp_close(pcb);
		return false;
	}

	// Allows TCP listener to listen for incomming traffic
	struct tcp_pcb* listenPcb = tcp_listen(pcb);
	if (listenPcb == NULL) {
		DEBUG_PRINTF("Failed to set up HTTP listening pcb\n");
		tcp_close(pcb);
		return false;
	}

	// Sets callback for handling connected clients after TCP handshake
	tcp_accept(listenPcb, http_accept_callback);

	return true;
}
