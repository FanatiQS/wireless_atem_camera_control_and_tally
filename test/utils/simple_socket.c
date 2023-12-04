#include <stdlib.h> // getenv, abort
#include <stddef.h> // size_t, NULL
#include <stdio.h> // fprintf, stderr, perror
#include <stdbool.h> // bool, true, false
#include <errno.h> // errno

#include <sys/socket.h> // socket, AF_INET, connect, struct sockaddr, setsockopt, SOL_SOCKET, SO_RCVTIMEO, SO_SNDTIMEO, ssize_t, send, recv
#include <poll.h> // struct pollfd, poll, nfds_t
#include <sys/time.h> // struct timeval tv
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons, inet_addr

#include "./simple_socket.h"

// Number of seconds a send or recv call waits before timing out
#ifndef SOCKET_TIMOEUT
#define SOCKET_TIMEOUT (20)
#endif // SOCKET_TIMEOUT

// Creates a network socket
int simple_socket_create(int type) {
	// Creates network socket
	int sock = socket(AF_INET, type, 0);
	if (sock == -1) {
		perror("Failed to create socket");
		abort();
	}

	// Sets socket timeouts for reading and writing
	struct timeval tv = { .tv_sec = SOCKET_TIMEOUT };
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

// Connects socket to device address from environment variable
void simple_socket_connect(int sock, int port, const char* envKey) {
	// Gets ip address from environment variable
	char* addr = getenv(envKey);
	if (addr == NULL) {
		fprintf(stderr, "Environment variable %s not defined\n", envKey);
		abort();
	}

	// Connects client socket to server address and port
	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = inet_addr(addr)
	};
	if (connect(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect client socket to server");
		abort();
	}
}

// Sends a buffer over the socket
void simple_socket_send(int sock, void* buf, size_t len) {
	// Ensures there is a buffer to send
	if (len == 0) {
		fprintf(stderr, "Unable to send 0 length buffer\n");
		abort();
	}

	// Sends the data
	ssize_t sendLen = send(sock, buf, len, 0);
	if (sendLen == -1) {
		perror("Failed to send data");
		abort();
	}

	// Rejects if sent length does not match length of buffer
	if (sendLen != (ssize_t)len) {
		fprintf(stderr, "Unexpected return value from send: %zd, %zu\n", sendLen, len);
		abort();
	}
}

// Receives a buffer from the socket
size_t simple_socket_recv(int sock, void* buf, size_t size) {
	// Ensures there is a buffer to receive to
	if (size == 0) {
		fprintf(stderr, "Unable to recv to 0 length buffer\n");
		abort();
	}

	// Receives data to the buffer
	ssize_t recvLen = recv(sock, buf, size, 0);
	if (recvLen == -1) {
		perror("Failed to recv data");
		abort();
	}

	// Rejects on invalid return value from recv call
	if (recvLen < 0) {
		fprintf(stderr, "Unexpected return value from recv: %zd\n", recvLen);
		abort();
	}

	// Returns number of bytes received
	return (size_t)recvLen;
}

// Receives data expecting specific error
bool simple_socket_recv_error(int sock, int err, void* buf, size_t* len) {
	// Ensures there is a buffer to receive to
	if (*len == 0) {
		fprintf(stderr, "Unable to recv to 0 length buffer\n");
		abort();
	}

	// Receives potential data to the buffer expecting error
	ssize_t recvLen = recv(sock, buf, *len, 0);
	if (recvLen != -1) {
		*len = (size_t)recvLen;
		return false;
	}

	// Rejects non matching error
	if (errno != err) {
		perror("Unexpected error");
		abort();
	}

	return true;
}

// Awaits data or timeout for socket
int simple_socket_poll(int sock, int timeout) {
	struct pollfd fds = {
		.fd = sock,
		.events = POLLIN
	};
	int pollLen = poll(&fds, 1, timeout);
	if (pollLen == -1) {
		perror("Poll got an error");
		abort();
	}
	if (pollLen < 0 || pollLen > 1) {
		fprintf(stderr, "Unexpected return value from poll: %d\n", pollLen);
		abort();
	}
	return pollLen;
}
