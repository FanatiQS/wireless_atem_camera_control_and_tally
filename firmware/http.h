// Include guard
#ifndef HTTP_H
#define HTTP_H

#include <stdint.h> // uint8_t, uint16_t, int32_t

#include <lwip/tcp.h> // struct tcp_pcb
#include <lwip/pbuf.h> // struct pbuf

#include "./flash.h" // struct cache_t



// States for the streaming HTTP state machine parser
enum http_state {
	HTTP_STATE_METHOD_GET,
	HTTP_STATE_GET_ROOT,
	HTTP_STATE_METHOD_POST,
	HTTP_STATE_POST_ROOT_HEADER_NEXT,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_MISSING,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_KEY,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_VALUE,
	HTTP_STATE_POST_ROOT_HEADER_CONTENT_LENGTH_CRLF,
	HTTP_STATE_POST_ROOT_HEADER_FLUSH,
	HTTP_STATE_POST_ROOT_BODY_PREPARE,
	HTTP_STATE_POST_ROOT_BODY_KEYS,
	HTTP_STATE_POST_ROOT_BODY_SSID_KEY,
	HTTP_STATE_POST_ROOT_BODY_PSK_KEY,
	HTTP_STATE_POST_ROOT_BODY_NAME_KEY,
	HTTP_STATE_POST_ROOT_BODY_DEST_KEY,
	HTTP_STATE_POST_ROOT_BODY_ATEM_KEY,
	HTTP_STATE_POST_ROOT_BODY_DHCP_KEY,
	HTTP_STATE_POST_ROOT_BODY_LOCALIP_KEY,
	HTTP_STATE_POST_ROOT_BODY_NETMASK_KEY,
	HTTP_STATE_POST_ROOT_BODY_GATEWAY_KEY,
	HTTP_STATE_POST_ROOT_BODY_SSID_VALUE,
	HTTP_STATE_POST_ROOT_BODY_PSK_VALUE,
	HTTP_STATE_POST_ROOT_BODY_NAME_VALUE,
	HTTP_STATE_POST_ROOT_BODY_DEST_VALUE,
	HTTP_STATE_POST_ROOT_BODY_ATEM_VALUE,
	HTTP_STATE_POST_ROOT_BODY_DHCP_VALUE,
	HTTP_STATE_POST_ROOT_BODY_LOCALIP_VALUE,
	HTTP_STATE_POST_ROOT_BODY_NETMASK_VALUE,
	HTTP_STATE_POST_ROOT_BODY_GATEWAY_VALUE,
	HTTP_STATE_POST_ROOT_BODY_FLUSH,
	HTTP_STATE_DONE
};

// HTTP client context for streaming parser
struct http_t {
	struct tcp_pcb* pcb;
	struct pbuf* p;
	uint16_t index;
	uint16_t offset;
	union {
		const char* cmp; // Uses by http_cmp_* functions
		int hex; // Used by http_post_value_string and setup in http_post_key_string
		int responseState; // Used by http_respond
	};
	union {
		int remainingBodyLen;
		int stringEscapeIndex;
	};
	union {
		struct cache_t cache;
		struct {
			const char* errCode;
			const char* errBody;
		};
	};
	enum http_state state;
};



// Initializes HTTP configuration server
struct tcp_pcb* http_init(void);

#endif // HTTP_H
