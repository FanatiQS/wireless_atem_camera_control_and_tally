#include <stdlib.h> // abort, NULL, getenv
#include <stdio.h> // perror, fprintf, stderr, printf
#include <assert.h> // assert
#include <string.h> // strlen, memcmp

#include <sys/socket.h> // socket, AF_INET, SOCK_STREAM, recv, ssize_t, connect, send, shutdown
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons, inet_addr
#include <unistd.h> // close

#include "./http_sock.h"

#define HTTP_PORT (80)
#define BUF_LEN (1024)



// Creates a client socket connected to HTTP server at address from environment variable
int http_socket_create(void) {
	// Gets device address from environment variable
	char* addr = getenv("DEVICE_ADDR");
	if (addr == NULL) {
		fprintf(stderr, "Environment variable DEVICE_ADDR not defined\n");
		abort();
	}

	// Creates TCP socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Failed to create socket");
		abort();
	}

	// Connects client socket to server
	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(HTTP_PORT),
		.sin_addr.s_addr = inet_addr(addr)
	};
	if (connect(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect client socket to server");
		abort();
	}

	// Sets socket timeouts for reading and writing on
	struct timeval tv = { .tv_sec = 20 };
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
		perror("Failed to set recv timeout");
		abort();
	}
	if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv))) {
		perror("Failed to set send timeout");
		abort();
	}

	return sock;
}



// Sends HTTP buffer to server
void http_socket_write(int sock, const char* buf, size_t len) {
	assert(len > 0);
	ssize_t sendLen = send(sock, buf, len, 0);
	if (sendLen == -1) {
		perror("Failed to send data to server");
		abort();
	}
	assert(sendLen == (ssize_t)len);
}

// Sends HTTP string to server
void http_socket_send(int sock, const char* str) {
	http_socket_write(sock, str, strlen(str));
}



// Reads HTTP data from stream into buffer
size_t http_socket_recv(int sock, char* buf, size_t size) {
	assert(size >= 2);
	ssize_t len = recv(sock, buf, size - 1, 0);

	if (len == -1) {
		perror("Failed to recv data from server");
		abort();
	}
	assert(len >= 0);

	buf[len] = '\0';
	return (size_t)len;
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
void http_socket_recv_cmp_status_line(int sock, int code) {
	http_socket_recv_cmp(sock, "HTTP/1.1 ");
	http_socket_recv_cmp(sock, http_status(code));
	http_socket_recv_cmp(sock, "\r\n");
}



// Prints multiline string with clear start and stop
void http_print(char* buf) {
	printf("====START====\n%s\n====END====\n", buf);
}

// Reads HTTP data from stream and prints it with clear start and stop
void http_socket_recvprint(int sock) {
	char buf[BUF_LEN];
	http_socket_recv(sock, buf, sizeof(buf));
	http_print(buf);
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
