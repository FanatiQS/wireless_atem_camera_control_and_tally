#include <stdlib.h> // abort
#include <stdio.h> // perror

#include <sys/socket.h> // socket, AF_INET, SOCK_STREAM, recv, ssize_t
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // htons, inet_addr
#include <unistd.h> // close

// Creates a client socket connected to HTTP server at address
int http_socket_create(char* addr) {
	// Creates TCP socket
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("Failed to create socket");
		abort();
	}

	// Connects client socket to server
	struct sockaddr_in sockAddr = {
		.sin_family = AF_INET,
		.sin_port = htons(80),
		.sin_addr.s_addr = inet_addr(addr)
	};
	if (connect(sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect client socket to server");
		abort();
	}

	return sock;
}

// Waits for socket to be closed
void http_socket_await_close(int sock) {
	char buf[1024];
	ssize_t len;
	do {
		len = recv(sock, buf, sizeof(buf), 0);
		if (len == -1) {
			perror("Failed to recv data from server");
			abort();
		}
	} while (len);
}

// Closes HTTP client socket
void http_socket_close(int sock) {
	close(sock);
}
