// Include guard
#ifndef HTTP_RESPONDER_H
#define HTTP_RESPONDER_H

#include <stdbool.h> // bool

// States for the response writer state machine
enum http_response_state {
	HTTP_RESPONSE_STATE_NONE,
	HTTP_RESPONSE_STATE_ROOT,
	HTTP_RESPONSE_STATE_ERR,
	HTTP_RESPONSE_STATE_POST_ROOT,
	HTTP_RESPONSE_STATE_POST_ROOT_COMPLETE
};

bool http_respond(struct http_t* http);
void http_err(struct http_t* http, const char* status);
bool http_post_completed(struct http_t* http);
void http_post_err(struct http_t* http, const char* msg);

#endif // HTTP_RESPONDER_H
