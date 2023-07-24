// Include guard
#ifndef HTTP_H
#define HTTP_H

#include "./flash.h" // struct config_t



// States for the streaming HTTP state machine parser
enum http_state {
	HTTP_STATE_METHOD_GET,
	HTTP_STATE_GET_404,
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
	HTTP_STATE_POST_ROOT_BODY_IPSTATIC_KEY,
	HTTP_STATE_POST_ROOT_BODY_IPLOCAL_KEY,
	HTTP_STATE_POST_ROOT_BODY_IPMASK_KEY,
	HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_KEY,
	HTTP_STATE_POST_ROOT_BODY_SSID_VALUE,
	HTTP_STATE_POST_ROOT_BODY_PSK_VALUE,
	HTTP_STATE_POST_ROOT_BODY_NAME_VALUE,
	HTTP_STATE_POST_ROOT_BODY_DEST_VALUE,
	HTTP_STATE_POST_ROOT_BODY_ATEM_VALUE,
	HTTP_STATE_POST_ROOT_BODY_IPSTATIC_VALUE,
	HTTP_STATE_POST_ROOT_BODY_IPLOCAL_VALUE,
	HTTP_STATE_POST_ROOT_BODY_IPMASK_VALUE,
	HTTP_STATE_POST_ROOT_BODY_IPGATEWAY_VALUE,
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
		const char* cmp;
		uint8_t hex;
	};
	int32_t remainingBodyLen;
	struct cache_t cache;
	enum http_state state;
};



// Initializes HTTP configuration server
bool http_init(void);

#endif // HTTP_H
