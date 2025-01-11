#include <stdlib.h> // getenv, abort
#include <stddef.h> // size_t, NULL
#include <stdio.h> // fprintf, stderr, perror
#include <stdbool.h> // bool, true, false
#include <errno.h> // errno, EINPROGRESS

#include <sys/socket.h> // socket, AF_INET, connect, struct sockaddr, setsockopt, SOL_SOCKET, SO_RCVTIMEO, SO_SNDTIMEO, ssize_t, send, recv, bind
#include <poll.h> // struct pollfd, poll, nfds_t
#include <sys/time.h> // struct timeval
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY, in_addr_t
#include <arpa/inet.h> // htons, inet_addr
#include <unistd.h> // close

#include "./simple_socket.h" // SIMPLE_SOCKET_MAX_FD

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

	// Ensures socket is within SIMPLE_SOCKET_MAX_FD so it will be closed on caught abort for runner mode all
	if (sock >= SIMPLE_SOCKET_MAX_FD) {
		fprintf(stderr, "Socket was assigned too high: %d\n", sock);
		close(sock);
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

// Connects client socket to server address and port
void simple_socket_connect(int sock, uint16_t port, in_addr_t addr) {
	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = addr
	};
	if (connect(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) && errno != EINPROGRESS) {
		perror("Failed to connect client socket to server");
		abort();
	}
}

// Connects socket to device address from environment variable
void simple_socket_connect_env(int sock, uint16_t port, const char* envKey) {
	// Gets ip address from environment variable
	char* addrStr = getenv(envKey);
	if (addrStr == NULL) {
		fprintf(stderr, "Environment variable %s not defined\n", envKey);
		abort();
	}

	// Gets ip address type from environment variable value
	in_addr_t addr = inet_addr(addrStr);
	if (addr == (in_addr_t)(-1)) {
		fprintf(stderr, "Invalid IP address '%s' from %s\n", addrStr, envKey);
		abort();
	}

	// Connects to address parsed from environment variable
	simple_socket_connect(sock, port, addr);
}

// Binds server socket for listening to clients
void simple_socket_listen(int sock, uint16_t port) {
	char* addr_str = getenv("LISTEN_ADDR");
	in_addr_t addr = INADDR_ANY;
	if (addr_str != NULL) {
		addr = inet_addr(addr_str);
		if (addr == 0) {
			fprintf(stderr, "Invalid value for LISTEN_ADDR: %s\n", addr_str);
			abort();
		}
	}
	struct sockaddr_in sock_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
		.sin_addr.s_addr = addr
	};
	if (bind(sock, (struct sockaddr*)&sock_addr, sizeof(sock_addr))) {
		perror("Failed to bind server socket");
		abort();
	}
}

// Sends a buffer over the socket
void simple_socket_send(int sock, const void* buf, size_t len) {
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
bool simple_socket_poll(int sock, int timeout) {
	struct pollfd fds = {
		.fd = sock,
		.events = POLLIN
	};
	int poll_len = poll(&fds, 1, timeout);
	if (poll_len == -1) {
		perror("Poll got an error");
		abort();
	}
	if (poll_len < 0 || poll_len > 1) {
		fprintf(stderr, "Unexpected return value from poll: %d\n", poll_len);
		abort();
	}
	return poll_len == 1;
}
