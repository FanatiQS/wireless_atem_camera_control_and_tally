// Include guard
#ifndef HTTP_H
#define HTTP_H

#include <stdint.h> // uint8_t, uint16_t, int32_t

#include <lwip/tcp.h> // struct tcp_pcb
#include <lwip/pbuf.h> // struct pbuf

#include "./flash.h" // struct flash_cache



// States for the streaming HTTP state machine parser
enum http_state {
	HTTP_STATE_METHOD_GET,
	HTTP_STATE_METHOD_POST,
	HTTP_STATE_POST_ROOT_HEADER_NEXT,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_MISSING,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_KEY,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_CRLF,
	HTTP_STATE_POST_ROOT_HEADER_FLUSH,
	HTTP_STATE_POST_ROOT_BODY_PREPARE,
	HTTP_STATE_POST_ROOT_BODY_KEYS,
	HTTP_STATE_POST_ROOT_BODY_KEY_SSID,
	HTTP_STATE_POST_ROOT_BODY_KEY_PSK,
	HTTP_STATE_POST_ROOT_BODY_KEY_NAME,
	HTTP_STATE_POST_ROOT_BODY_KEY_DEST,
	HTTP_STATE_POST_ROOT_BODY_KEY_ATEM,
	HTTP_STATE_POST_ROOT_BODY_KEY_DHCP,
	HTTP_STATE_POST_ROOT_BODY_KEY_LOCALIP,
	HTTP_STATE_POST_ROOT_BODY_KEY_NETMASK,
	HTTP_STATE_POST_ROOT_BODY_KEY_GATEWAY,
	HTTP_STATE_POST_ROOT_BODY_VALUE_SSID,
	HTTP_STATE_POST_ROOT_BODY_VALUE_PSK,
	HTTP_STATE_POST_ROOT_BODY_VALUE_NAME,
	HTTP_STATE_POST_ROOT_BODY_VALUE_DEST,
	HTTP_STATE_POST_ROOT_BODY_VALUE_ATEM,
	HTTP_STATE_POST_ROOT_BODY_VALUE_DHCP,
	HTTP_STATE_POST_ROOT_BODY_VALUE_LOCALIP,
	HTTP_STATE_POST_ROOT_BODY_VALUE_NETMASK,
	HTTP_STATE_POST_ROOT_BODY_VALUE_GATEWAY,
	HTTP_STATE_POST_ROOT_BODY_FLUSH,
	HTTP_STATE_DONE
};

// HTTP client context for streaming parser
struct http_ctx {
	struct tcp_pcb* pcb;
	struct pbuf* p;
	uint16_t index;
	uint16_t offset;
	union {
		const char* cmp; // Uses by http_cmp_* functions
		int hex; // Used by http_post_value_string and setup in http_post_key_string
		int response_state; // Used by http_respond
	};
	union {
		int remaining_body_len;
		int string_escape_index;
	};
	union {
		struct flash_cache cache;
		struct {
			const char* err_code;
			const char* err_body;
		};
	};
	enum http_state state;
};



// Initializes HTTP configuration server
struct tcp_pcb* http_init(void);

#endif // HTTP_H
