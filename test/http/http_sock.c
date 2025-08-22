#include <stdlib.h> // abort, NULL
#include <stdio.h> // perror, fprintf, stderr, printf, snprintf, stdout
#include <assert.h> // assert
#include <string.h> // strlen, memcmp

#include <sys/socket.h> // SOCK_STREAM, shutdown
#include <unistd.h> // close

#include "../utils/simple_socket.h" // simple_socket_create, simple_socket_connect_env, simple_socket_send, simple_socket_recv, simple_socket_recv_error
#include "../utils/logs.h" // logs_print_string, logs_find
#include "./http_sock.h"

#define HTTP_PORT (80)
#define BUF_LEN (1024)



// Creates a client socket connected to HTTP server at address from environment variable
int http_socket_create(void) {
	int sock = simple_socket_create(SOCK_STREAM);
	simple_socket_connect_env(sock, HTTP_PORT, "DEVICE_ADDR");
	return sock;
}



// Sends HTTP buffer to server
void http_socket_send_buffer(int sock, const char* buf, size_t len) {
	assert(len == strlen(buf));
	if (logs_find("http_send")) {
		printf("HTTP transmit:\n");
		logs_print_string(stdout, buf, false);
	}
	simple_socket_send(sock, buf, len);
}

// Sends HTTP string to server
void http_socket_send_string(int sock, const char* str) {
	http_socket_send_buffer(sock, str, strlen(str));
}



// States for chunked HTTP receive string logging
static bool http_socket_recv_chunking_enabled = false;
static bool http_socket_recv_chunking_started = false;

// Enables logging multiple HTTP receives as a single chunk instead of individually logged packets
void http_socket_recv_chunking_start(void) {
	http_socket_recv_chunking_enabled = true;
}

// Terminates chunked HTTP receive log
void http_socket_recv_chunking_end(void) {
	http_socket_recv_chunking_enabled = false;
	http_socket_recv_chunking_started = false;
	if (logs_find("http_recv")) {
		printf("\n");
	}
}

// Reads HTTP data from stream into buffer
size_t http_socket_recv(int sock, char* buf, size_t size) {
	assert(size >= 2);
	size_t len = simple_socket_recv(sock, buf, size - 1);
	buf[len] = '\0';

	if (logs_find("http_recv") && len > 0) {
		if (!http_socket_recv_chunking_started) {
			printf("HTTP receive:\n");
			if (http_socket_recv_chunking_enabled) {
				http_socket_recv_chunking_started = true;
			}
		}
		logs_print_string(stdout, buf, http_socket_recv_chunking_enabled);
	}

	return len;
}

// Flushes data from stream and returns number of bytes flushed
size_t http_socket_recv_len(int sock) {
	char buf[BUF_LEN];
	return http_socket_recv(sock, buf, sizeof(buf));
}



// Ensures data in stream is the same as string
void http_socket_recv_cmp(int sock, const char* cmp_buf) {
	char recv_buf[BUF_LEN];
	size_t cmp_len = strlen(cmp_buf);
	assert(cmp_len < sizeof(recv_buf));
	size_t recv_len = http_socket_recv(sock, recv_buf, cmp_len + 1);

	if (recv_len != cmp_len) {
		fprintf(stderr, "Received data does not have the same length: %zu %zu\n", recv_len, cmp_len);
	}
	else {
		if (memcmp(cmp_buf, recv_buf, cmp_len) == 0) return;
		fprintf(stderr, "Buffer mismatch\n");
	}

	fprintf(stderr, "====RECV====\n%s\n====END====\n", recv_buf);
	fprintf(stderr, "====CMP====\n%s\n====END====\n", cmp_buf);
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

// Flushes HTTP stream data until match is reached
void http_socket_recv_until(int sock, char* match) {
	http_socket_recv_chunking_start();

	char buf[2];
	size_t index = 0;
	do {
		size_t recved = http_socket_recv(sock, buf, sizeof(buf));
		if (recved == 0) {
			http_socket_recv_chunking_end();
			fprintf(stderr, "End of stream\n");
			abort();
		}
		assert(recved == 1);
		if (buf[0] == match[index]) {
			index++;
		}
		else {
			index = 0;
		}
	} while (index < strlen(match));

	http_socket_recv_chunking_end();
}

// Ensures data in stream is a valid HTTP 1.1 status line
void http_socket_recv_cmp_status(int sock, int code) {
	http_socket_recv_cmp(sock, "HTTP/1.1 ");
	http_socket_recv_cmp(sock, http_status(code));
	http_socket_recv_until(sock, "\r\n\r\n");
}



// Reads HTTP data from stream and prints it with clear start and stop
void http_socket_recv_print(int sock) {
	char buf[BUF_LEN];
	http_socket_recv(sock, buf, sizeof(buf));
	logs_print_string(stdout, buf, false);
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
	size_t buf_len = sizeof(buf);
	if (simple_socket_recv_error(sock, err, buf, &buf_len)) return;
	if (buf_len == 0) {
		fprintf(stderr, "Socket unexpectedly closed when expecting err: %d\n", err);
	}
	else {
		fprintf(stderr, "Socket unexpectedly got response data: %zu\n", buf_len);
		logs_print_string(stderr, buf, false);
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
void http_socket_body_send_buffer(int sock, const char* body, size_t body_len) {
	char req_buf[BUF_LEN];
	int req_len = snprintf(
		req_buf,
		sizeof(req_buf),
		"POST / HTTP/1.1\r\nContent-Length: %zu\r\n\r\n%s",
		body_len, body
	);
	assert((size_t)req_len < sizeof(req_buf));
	assert(req_len > 0);
	http_socket_send_buffer(sock, req_buf, (size_t)req_len);
}

// Sends HTTP POST string body to server
void http_socket_body_send_string(int sock, const char* body) {
	http_socket_body_send_buffer(sock, body, strlen(body));
}
