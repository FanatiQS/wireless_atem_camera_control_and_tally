#include <stdbool.h> // bool, true, false
#include <stddef.h> // size_t, NULL
#include <stdint.h> // uint8_t, uint16_t, uint32_t, int32_t, INT32_MAX
#include <string.h> // memcpy
#include <ctype.h> // tolower, isdigit
#include <string.h> // strlen

#include <lwip/tcp.h> // struct tcp_pcb, tcp_new, tcp_bind, tcp_close, tcp_listen, tcp_accept, tcp_nagle_disable, tcp_poll, tcp_recv, tcp_sent, tcp_err, tcp_recved, tcp_arg
#include <lwip/err.h> // err_t, ERR_OK, ERR_VAL
#include <lwip/pbuf.h> // struct pbuf, pbuf_free
#include <lwip/ip_addr.h> // IP_ADDR_ANY
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/mem.h> // mem_malloc, mem_free

#include "./user_config.h" // DEBUG_HTTP
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF, DEBUG_HTTP_PRINTF
#include "./flash.h" // CACHE_SSID, CACHE_PSK, CACHE_NAME, flash_cache_read
#include "./http.h" // enum http_state, struct http_ctx, http_init
#include "./http_respond.h" // http_respond, http_err, http_post_err, enum http_response_state, http_post_completed

// Default port for unencrypted HTTP traffic
#define HTTP_PORT 80

// The number of TCP coarse grained timer shots before the TCP socket is closed automatically
#define HTTP_POLL_TIMEOUT 10

// Converts percent encoded HEX character set to decimal value
#define HEX_IDLE  (1)
#define HEX_READY (2)
#define HEX_TO_DEC(c) ((c & 0x0f) + 9 * (c >> 6))

// Maximum and minimum camera id (dest) values
#define DEST_MIN (1)
#define DEST_MAX (254)



// Checks if HTTP stream has more data available
static bool http_char_available(struct http_ctx* http) {
	if (http->index == http->p->len) {
		if (http->p->len == http->p->tot_len) return false;
		http->p = http->p->next;
		http->index = 0;
	}
	return true;
}

// Peeks at the next character in the HTTP stream without consuming it
static inline char http_char_peek(struct http_ctx* http) {
	return ((char*)http->p->payload)[http->index];
}

// Consumes the next character from the HTTP stream
static char http_char_consume(struct http_ctx* http) {
	char c = http_char_peek(http);
	http->index++;
	return c;
}



// Moves to the next character in the comparison and checks if it has reached its terminator
static inline bool http_cmp_success(struct http_ctx* http, const char* cmp) {
	http->offset++;
	if (cmp[http->offset] != '\0') {
		return false;
	}
	http->offset = 0;
	return true;
}

// Compares up to the current point from previously half-matched string
static bool http_cmp_back(struct http_ctx* http, const char* cmp) {
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
static bool http_cmp(struct http_ctx* http, const char* cmp) {
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
static bool http_cmp_case_insensitive(struct http_ctx* http, const char* cmp) {
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
static inline bool http_cmp_incomplete(struct http_ctx* http) {
	return (http->cmp == NULL);
}

// Consumes characters up to next occurance of a substring in HTTP stream
static bool http_find(struct http_ctx* http, const char* cmp) {
	while (http_char_available(http)) {
		// Resets back to beginning of compare string if comparison failes
		if (http_char_consume(http) != cmp[http->offset]) {
			http->offset = 0;
		}
		// Successfully returns if end of comparison string is reached
		else if (http_cmp_success(http, cmp)) {
			return true;
		}
	}

	return false;
}



// Compares HTTP stream against POST body key
static bool http_post_key(struct http_ctx* http, const char* key) {
	if (!http_cmp(http, key)) return false;
	http->remaining_body_len -= strlen(key);
	DEBUG_HTTP_PRINTF("Found POST body key '%s' for %p\n", key, (void*)http->pcb);
	return true;
}

// Compares HTTP stream against POST body key leading to a string value
static bool http_post_key_string(struct http_ctx* http, const char* key) {
	if (!http_post_key(http, key)) return false;
	http->hex = HEX_IDLE;
	return true;
}

// Checks if the last failing POST body key comparison failed due to running out of data in the HTTP stream
static bool http_post_key_incomplete(struct http_ctx* http) {
	if (!http_cmp_incomplete(http)) return false;
	return (http->remaining_body_len > http->offset);
}



// Completes integer parsing on terminator character
static inline bool http_post_delimited(struct http_ctx* http, const char c) {
	if (c != '&') return false;
	http->remaining_body_len--;
	http->state = HTTP_STATE_POST_ROOT_BODY_KEYS;
	return true;
}

// Increments integer with numeric character from HTTP stream
static inline bool http_post_octet(struct http_ctx* http, const char c, uint8_t* addr) {
	uint8_t num = (uint8_t)c - '0';

	// Appends the number character to the integer pointer and validates it will not overflow
	if (*addr > (UINT8_MAX / 10)) return false;
	*addr *= 10;
	if (*addr > (UINT8_MAX - num)) return false;
	*addr += num;
	http->remaining_body_len--;

	return true;
}



// Writes POST body string to HTTP clients cache
static bool http_post_value_string(struct http_ctx* http, char* addr, size_t addr_size) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		http->remaining_body_len--;

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
			if (http->offset < addr_size) addr[http->offset] = '\0';
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
		if (http->offset >= addr_size) {
			http_post_err(http, "String value too long");
			return false;
		}

		// Copies over streamed character to cache
		addr[http->offset] = c;
		http->offset++;
	}

	// Stops copying string when entire body is processed
	if (http->offset < addr_size) addr[http->offset] = '\0';

	// Completes HTTP parsing if end of body is reached
#if !DEBUG_HTTP
	http_post_completed(http);
	return false;
#else // !DEBUG_HTTP
 	// Goes back through HTTP state machine again to print cached address
 	return http_post_completed(http);
#endif // !DEBUG_HTTP
}

// Writes POST body integer to HTTP clients cache
static bool http_post_value_uint8(struct http_ctx* http, uint8_t* addr, size_t min, size_t max) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		if (!isdigit(c)) {
			if (http_post_delimited(http, c)) {
				return true;
			}
			http_post_err(http, "Invalid character in integer");
			return false;
		}
		if (!http_post_octet(http, c, addr) || *addr < min || *addr > max) {
			http_post_err(http, "Integer value out of range");
			return false;
		}
	}

	// Completes HTTP parsing if end of body is reached
#if !DEBUG_HTTP
	http_post_completed(http);
	return false;
#else // !DEBUG_HTTP
	// Goes back through HTTP state machine again to print cached address
	return http_post_completed(http);
#endif // !DEBUG_HTTP
}

// Writes POST body IPV4 address to HTTP clients cache
static bool http_post_value_ip(struct http_ctx* http, uint32_t* addr) {
	while (http_char_available(http)) {
		char c = http_char_consume(http);
		if (isdigit(c)) {
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
			http->remaining_body_len--;
		}
		else if (http_post_delimited(http, c) && http->offset == 3) {
			http->offset = 0;
			return true;
		}
		else {
			http_post_err(http, "Invalid IPV4 address");
			return false;
		}
	}

	// Rejects IP address with less than 4 segments
	if (http->offset < 3) {
		http_post_err(http, "Invalid IPV4 address");
		return false;
	}

	// Completes configuration if there is no more pending data
#if !DEBUG_HTTP
	http_post_completed(http);
	return false;
#else // !DEBUG_HTTP
	// Goes back through HTTP state machine again to print cached address
	return http_post_completed(http);
#endif // !DEBUG_HTTP
}

// Writes POST body flag to HTTP clients cache
static bool http_post_value_flag(struct http_ctx* http, uint8_t* flags, int mask) {
	// Continues after already processed boolean character value
	if (http->offset != 0) {
		if (!http_char_available(http)) { // @todo is this needed? should only get here if this is the first character and a pbuf should never be empty, right?
			return false;
		}
		http->offset = 0;
	}
	// Comes back here if HTTP stream is empty and expects more body data
	else if (!http_char_available(http)) {
		if (http->remaining_body_len <= 0) {
			http_post_err(http, "Empty boolean value");
		}
		return false;
	}
	// Parses boolean character
	else {
		char c = http_char_consume(http);
		if (c == '0') {
			*flags &= ~mask;
		}
		else if (c == '1') {
			*flags |= mask;
		}
		else {
			http_post_err(http, "Invalid boolean value, only accepts '1' or '0'");
			return false;
		}
		http->remaining_body_len--;

		// Comes back to parse expected delimiter if HTTP stream is empty and expects more body data
		if (!http_char_available(http)) {
			if (http_post_completed(http)) {
#if !DEBUG_HTTP
				return false;
#else // !DEBUG_HTTP
				// Goes back through HTTP state machine again to print cached address
				return true;
#endif // !DEBUG_HTTP
			}
			http->offset = 1;
			return false;
		}
	}

	// Completes configuration at delimiter
	if (!http_post_delimited(http, http_char_consume(http))) {
		http_post_err(http, "Invalid boolean value, only accepts '1' or '0'");
		return false;
	}
	http->remaining_body_len--;
	return true;
}



/*
 * The function `http_parse` works as a big jump table where each case represents a state.
 * A state is a parsing step that has to be able to be resumed if the HTTP stream runs out of data.
 * When a state has completed its parsing, it can either, based on the parsing result:
 *     fall through: To continue parsing the next state followed in the switch case.
 *     continue (or break): To jump to manually set state that is somewhere else in the control flow.
 *     return: If there is no more data to parse or response is sent.
 */
static inline void http_parse(struct http_ctx* http, struct pbuf* p) {
	http->p = p;
	http->index = 0;

#if DEBUG_HTTP
	DEBUG_HTTP_PRINTF("Received data from client %p:\n", (void*)http->pcb);
	DEBUG_PRINTF("========START========\n");
	while (true) {
		DEBUG_PRINTF("%.*s\n", p->len, (char*)p->payload);
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
		// Responds with root HTML to all HTTP GET requests
		/* FALLTHROUGH */
		case HTTP_STATE_GET_ROOT: {
			if (!flash_cache_read(&http->cache)) {
				http_err(http, "500 Internal Server Error");
				return;
			}
			http->response_state = HTTP_RESPONSE_STATE_ROOT;
			http->state = HTTP_STATE_DONE;
			http_respond(http);
			DEBUG_HTTP_PRINTF("Responding to unspecified GET request from %p\n", (void*)http->pcb);
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
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_HEADER_NEXT: {
			if (!http_find(http, "\r\n")) {
				http->state = HTTP_STATE_POST_ROOT_HEADER_NEXT;
				return;
			}
		}
		// Rejects client if content length header is missing from POST request
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
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
			http->remaining_body_len = 0;
		}
		// Gets content length value from HTTP stream
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE: {
			while (true) {
				if (!http_char_available(http)) {
					http->state = HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE;
					return;
				}
				char c = http_char_peek(http);
				uint8_t num = (uint8_t)c - '0';
				if (num < 10) {
					if ((http->remaining_body_len + num) > ((INT32_MAX / 10) + (INT32_MAX % 10))) {
						http_err(http, "413 Payload Too Large");
						DEBUG_HTTP_PRINTF("Content length too large from %p\n", (void*)http->pcb);
						return;
					}
					http->remaining_body_len *= 10;
					http->remaining_body_len += num;
				}
				else if (c != ' ') {
					break;
				}
				http->index++;
			}
		}
		// Rejects client if content length value has non numeric characers before line termination
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_HEADER_FLUSH: {
			if (!http_find(http, "\r\n\r\n")) {
				http->state = HTTP_STATE_POST_ROOT_HEADER_FLUSH;
				return;
			}
			DEBUG_HTTP_PRINTF(
				"Got a content length of %d bytes from %p\n",
				http->remaining_body_len, (void*)http->pcb
			);
		}
		// Reads current configuration into clients cache
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_PREPARE: {
			if (!flash_cache_read(&http->cache)) {
				http_err(http, "500 Internal Server Error");
				return;
			}
		}
		// Parses next HTTP body key
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_KEYS:
		// Checks if the next POST body key is for wlan ssid
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
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
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_DEST_KEY: {
			if (http_post_key(http, "dest=")) {
				if (http->remaining_body_len <= 0) {
					http_post_err(http, "Invalid empty integer");
					return;
				}
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
		/* FALLTHROUGH */
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
		// Checks if the next POST body key is for DHCP or static ip
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_DHCP_KEY: {
			if (http_post_key(http, "dhcp=")) {
				http->state = HTTP_STATE_POST_ROOT_BODY_DHCP_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_DHCP_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip local address
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_LOCALIP_KEY: {
			if (http_post_key(http, "localip=")) {
				http->cache.config.localip = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_LOCALIP_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_LOCALIP_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip netmask
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_NETMASK_KEY: {
			if (http_post_key(http, "netmask=")) {
				http->cache.config.netmask = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_NETMASK_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_NETMASK_KEY;
				return;
			}
		}
		// Checks if the next POST body key is for static ip gateway
		/* FALLTHROUGH */
		case HTTP_STATE_POST_ROOT_BODY_GATEWAY_KEY: {
			if (http_post_key(http, "gateway=")) {
				http->cache.config.gateway = 0;
				http->state = HTTP_STATE_POST_ROOT_BODY_GATEWAY_VALUE;
				continue;
			}
			if (http_post_key_incomplete(http)) {
				http->state = HTTP_STATE_POST_ROOT_BODY_GATEWAY_KEY;
				return;
			}
		}
		// Rejects client on invalid POST body key
		/* FALLTHROUGH */
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
			if (!http_post_value_string(http, (char*)http->cache.CACHE_SSID, sizeof(http->cache.CACHE_SSID))) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'ssid' to \"%.*s\" for %p\n",
				sizeof(http->cache.CACHE_SSID), http->cache.CACHE_SSID,
				(void*)http->pcb
			);
			continue;
		}
		// Transfers wlan psk from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_PSK_VALUE: {
			if (!http_post_value_string(http, (char*)http->cache.CACHE_PSK, sizeof(http->cache.CACHE_PSK))) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'psk' to \"%.*s\" for %p\n",
				sizeof(http->cache.CACHE_PSK), http->cache.CACHE_PSK,
				(void*)http->pcb
			);
			continue;
		}
		// Transfers device name from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_NAME_VALUE: {
			if (!http_post_value_string(http, (char*)http->cache.CACHE_NAME, sizeof(http->cache.CACHE_NAME))) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'name' to \"%.*s\" for %p\n",
				sizeof(http->cache.CACHE_NAME), http->cache.CACHE_NAME,
				(void*)http->pcb
			);
			continue;
		}
		// Transfers camera id (dest) from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_DEST_VALUE: {
			if (!http_post_value_uint8(http, &http->cache.config.dest, DEST_MIN, DEST_MAX)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'dest' to %d for %p\n",
				http->cache.config.dest,
				(void*)http->pcb
			);
			continue;
		}
		// Transfers ATEM address from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_ATEM_VALUE: {
			if (!http_post_value_ip(http, &http->cache.config.atemAddr)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'atem' to " IP_FMT " for %p\n",
				IP_VALUE(http->cache.config.atemAddr),
				(void*)http->pcb
			);
			continue;
		}
		// Transfers static ip flag from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_DHCP_VALUE: {
			if (!http_post_value_flag(http, &http->cache.config.flags, CONF_FLAG_DHCP)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'dhcp' to %s for %p\n",
				(http->cache.config.flags & CONF_FLAG_DHCP) ? "ON" : "OFF",
				(void*)http->pcb
			);
			continue;
		}
		// Transfers static ip local address from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_LOCALIP_VALUE: {
			if (!http_post_value_ip(http, &http->cache.config.localip)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'localip' to " IP_FMT " for %p\n",
				IP_VALUE(http->cache.config.localip),
				(void*)http->pcb
			);
			continue;
		}
		// Transfers static ip netmask from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_NETMASK_VALUE: {
			if (!http_post_value_ip(http, &http->cache.config.netmask)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'netmask' to " IP_FMT " for %p\n",
				IP_VALUE(http->cache.config.netmask),
				(void*)http->pcb
			);
			continue;
		}
		// Transfers static ip gateway from POST body to client cache
		case HTTP_STATE_POST_ROOT_BODY_GATEWAY_VALUE: {
			if (!http_post_value_ip(http, &http->cache.config.gateway)) return;
			DEBUG_HTTP_PRINTF(
				"Updated 'gateway' to " IP_FMT " for %p\n",
				IP_VALUE(http->cache.config.gateway),
				(void*)http->pcb
			);
			continue;
		}
		// Ignores segment from client that has already sent response to request
		case HTTP_STATE_DONE: return;
	}
}



// Closes HTTP connection and prevents dispaching invalid events after close
static inline err_t http_close(struct http_ctx* http) {
	err_t err = tcp_close(http->pcb);
	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to close HTTP TCP pcb %p: %d", (void*)http->pcb, (int)err);
		return err;
	}

	tcp_poll(http->pcb, NULL, 0);
	tcp_recv(http->pcb, NULL);
	tcp_err(http->pcb, NULL);
	tcp_sent(http->pcb, NULL);
	mem_free(http);

	return ERR_OK;
}

// Closes HTTP connection when all response data is sent
static err_t http_sent_callback(void* arg, struct tcp_pcb* pcb, uint16_t len) {
	LWIP_UNUSED_ARG(pcb);
	LWIP_UNUSED_ARG(len);
	struct http_ctx* http = (struct http_ctx*)arg;

	DEBUG_HTTP_PRINTF("Sent %d bytes of data to client %p\n", len, (void*)pcb);

	// Sends more unsent response data
	if (http_respond(http)) return ERR_OK;

	DEBUG_HTTP_PRINTF("Closing client %p\n", (void*)pcb);

	// Closes connection when response is completely sent
	return http_close(http);
}

// Processes the received TCP packet data
static err_t http_recv_callback(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
	struct http_ctx* http = (struct http_ctx*)arg;

	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("HTTP client %p got recv error: %d\n", (void*)pcb, (int)err);
		if (p != NULL && err == ERR_MEM) pbuf_free(p);
		return err;
	}

	// Closes the TCP connection on client request
	if (p == NULL) {
		DEBUG_HTTP_PRINTF("Closed client %p\n", (void*)pcb);
		return http_close(http);
	}

	// Processes the TCP packet data
	http_parse(http, p);
	tcp_recved(pcb, p->tot_len);
	pbuf_free(p);

	return ERR_OK;
}

// Frees argument memory on TCP socket error
static void http_err_callback(void* arg, err_t err) {
	LWIP_UNUSED_ARG(err);
	DEBUG_ERR_PRINTF("HTTP client %p got an error: %d\n", (void*)((struct http_ctx*)arg)->pcb, (int)err);
	mem_free(arg);
}

// Closes TCP connection after timeout
static err_t http_drop_callback(void* arg, struct tcp_pcb* pcb) {
	LWIP_UNUSED_ARG(pcb);
	struct http_ctx* http = (struct http_ctx*)arg;
	if (http_respond(http)) return ERR_OK;
	DEBUG_HTTP_PRINTF("Dropping client %p due to inactivity\n", (void*)pcb);
	return http_close(http);
}

// Accepts TCP connection for HTTP configuration server
static err_t http_accept_callback(void* arg, struct tcp_pcb* newpcb, err_t err) {
	LWIP_UNUSED_ARG(arg);

	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("HTTP server received accept error: %d\n", (int)err);
		return err;
	}

	// Creates context to use when parsing HTTP data stream
	struct http_ctx* http = mem_malloc(sizeof(struct http_ctx));
	if (http == NULL) {
		DEBUG_ERR_PRINTF("Failed to allocate HTTP struct for new client\n");
		return ERR_MEM;
	}
	tcp_arg(newpcb, http);
	http->offset = 0;
	http->state = (enum http_state)0;
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
struct tcp_pcb* http_init(void) {
	// Creates protocol control buffer for HTTP servers TCP listener
	struct tcp_pcb* pcb = tcp_new();
	if (pcb == NULL) {
		DEBUG_ERR_PRINTF("Failed to create HTTP pcb\n");
		return NULL;
	}

	// Binds TCP listener to allow traffic from any address to default HTTP port
	err_t err = tcp_bind(pcb, IP_ADDR_ANY, HTTP_PORT);
	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to bind HTTP listen pcb: %d\n", (int)err);
		tcp_close(pcb);
		return NULL;
	}

	// Allows TCP listener to listen for incomming traffic
	struct tcp_pcb* pcb_listen = tcp_listen(pcb);
	if (pcb_listen == NULL) {
		DEBUG_ERR_PRINTF("Failed to set up HTTP listening pcb\n");
		tcp_close(pcb);
		return NULL;
	}

	// Sets callback for handling connected clients after TCP handshake
	tcp_accept(pcb_listen, http_accept_callback);

	return pcb_listen;
}
