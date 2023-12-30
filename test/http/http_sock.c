#include <stdlib.h> // abort, NULL
#include <stdio.h> // perror, fprintf, stderr, printf, snprintf, stdout
#include <assert.h> // assert
#include <string.h> // strlen, memcmp

#include <sys/socket.h> // SOCK_STREAM, shutdown
#include <unistd.h> // close

#include "../utils/simple_socket.h" // simple_socket_create, simple_socket_connect, simple_socket_send, simple_socket_recv, simple_socket_recv_error
#include "../utils/logs.h" // logs_print_string, logs_find
#include "./http_sock.h"

#define HTTP_PORT (80)
#define BUF_LEN (1024)



// Creates a client socket connected to HTTP server at address from environment variable
int http_socket_create(void) {
	int sock = simple_socket_create(SOCK_STREAM);
	simple_socket_connect(sock, HTTP_PORT, "DEVICE_ADDR");
	return sock;
}



// Sends HTTP buffer to server
void http_socket_send_buffer(int sock, const char* buf, size_t len) {
	if (logs_find("http_send")) {
		logs_print_string(stdout, buf);
	}
	simple_socket_send(sock, buf, len);
}

// Sends HTTP string to server
void http_socket_send_string(int sock, const char* str) {
	if (logs_find("http_send")) {
		logs_print_string(stdout, str);
	}
	http_socket_send_buffer(sock, str, strlen(str));
}



// Reads HTTP data from stream into buffer
size_t http_socket_recv(int sock, char* buf, size_t size) {
	assert(size >= 2);
	size_t len = simple_socket_recv(sock, buf, size - 1);
	buf[len] = '\0';

	if (logs_find("http_recv")) {
		logs_print_string(stdout, buf);
	}

	return len;
}

// Flushes data from stream and returns number of bytes flushed
size_t http_socket_recv_len(int sock) {
	char buf[BUF_LEN];
	return http_socket_recv(sock, buf, sizeof(buf));
}



// Ensures data in stream is the same as string
void http_socket_recv_cmp(int sock, const char* cmpBuf) {
	char recvBuf[BUF_LEN];
	size_t cmpLen = strlen(cmpBuf);
	size_t recvLen = http_socket_recv(sock, recvBuf, cmpLen + 1);

	if (recvLen != cmpLen) {
		fprintf(stderr, "Received data does not have the same length: %zu %zu\n", recvLen, cmpLen);
	}
	else {
		if (memcmp(cmpBuf, recvBuf, cmpLen) == 0) return;
		fprintf(stderr, "Buffer mismatch\n");
	}

	fprintf(stderr, "====RECV====\n%s\n====END====\n", recvBuf);
	fprintf(stderr, "====CMP====\n%s\n====END====\n", cmpBuf);
	abort();
}

// Gets the HTTP status code with message as string from integer
char* http_status(int code) {
	switch (code) {
		case 200: return "200 OK";
		case 400: return "400 Bad Request";
		case 405: return "405 Method Not Allowed";
		case 404: return "404 Not Found";
		case 411: return "411 Length Required";
		case 413: return "413 Payload Too Large";
		default: {
			fprintf(stderr, "HTTP status code not implemented in test library\n");
			abort();
		}
	}
}

// Ensures data in stream is a valid HTTP 1.1 status line
void http_socket_recv_cmp_status(int sock, int code) {
	// Ensures HTTP version and status code
	http_socket_recv_cmp(sock, "HTTP/1.1 ");
	http_socket_recv_cmp(sock, http_status(code));

	// Ensures double CRLF after potential headers
	size_t index = 0;
	do {
		char buf[2];
		assert(http_socket_recv(sock, buf, 2) == 1);
		index = (buf[0] == "\r\n\r\n"[index]) ? index + 1 : 0;
	} while (index < 4);
}



// Reads HTTP data from stream and prints it with clear start and stop
void http_socket_recv_print(int sock) {
	char buf[BUF_LEN];
	http_socket_recv(sock, buf, sizeof(buf));
	logs_print_string(stdout, buf);
}



// Ensures the connection is closed
void http_socket_recv_close(int sock) {
	char buf[BUF_LEN];
	if (http_socket_recv(sock, buf, sizeof(buf)) == 0) return;
	fprintf(stderr, "Expected socket to be closed by server\n\trecvd: %s\n", buf);
	abort();
}

// Flushes data available in stream
void http_socket_recv_flush(int sock) {
	char buf[BUF_LEN];
	if (http_socket_recv(sock, buf, sizeof(buf)) > 0) return;
	fprintf(stderr, "Expected socket to contain data to flush\n\trecved: %s\n", buf);
	abort();
}

// Ensures the next recv gets an error
void http_socket_recv_error(int sock, int err) {
	char buf[BUF_LEN];
	size_t buflen = sizeof(buf);
	if (simple_socket_recv_error(sock, err, buf, &buflen)) return;
	if (buflen == 0) {
		fprintf(stderr, "Socket unexpectedly closed when expecting err: %d\n", err);
	}
	else {
		fprintf(stderr, "Socket unexpectedly got response data: %zd\n", buflen);
		logs_print_string(stderr, buf);
	}
	abort();
}



// Closes HTTP client socket
void http_socket_close(int sock) {
	shutdown(sock, SHUT_WR);
	char buf[BUF_LEN];
	if (http_socket_recv(sock, buf, sizeof(buf)) > 0) {
		fprintf(stderr, "Expected socket to close, but got data\n\t%s\n", buf);
		abort();
	}
	close(sock);
}



// Sends HTTP POST buffer body to server
void http_socket_body_send_buffer(int sock, const char* body, size_t bodyLen) {
	char reqBuf[BUF_LEN];
	int reqLen = snprintf(reqBuf, sizeof(reqBuf), "POST / HTTP/1.1\r\nContent-Length: %zu\r\n\r\n%s", bodyLen, body);
	assert((size_t)reqLen < sizeof(reqBuf));
	http_socket_send_buffer(sock, reqBuf, reqLen);
}

// Sends HTTP POST string body to server
void http_socket_body_send_string(int sock, const char* body) {
	http_socket_body_send_buffer(sock, body, strlen(body));
}
