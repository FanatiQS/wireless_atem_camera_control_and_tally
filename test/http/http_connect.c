#include <stdio.h> // printf
#include <stdlib.h> // getenv, atio

#include <sys/socket.h> // recv

#include "../utils/http_sock.h" // http_socket_create, http_socket_send, http_socket_recv_len, http_socket_close, http_socket_recv_flush
#include "../utils/runner.h" // RUN_TEST

int main(void) {
	// Number of iterations to trigger memory leak if available
	char* itersEnvStr = getenv("HTTP_CONNECTION_ITERS");
	int iters = (itersEnvStr) ? atoi(itersEnvStr) : 1000;

	// Ensure there are no memory leaks for normal HTTP requests
	RUN_TEST(
		printf("Test basic requests\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GET / HTTP/1.0\r\n\r\n");
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);

	// Ensures there are no memory leaks when sending after server close
	RUN_TEST(
		printf("Test write after basic request\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GET / HTTP/1.0\r\n\r\n");
			http_socket_send(sock, "X");
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);

	// @todo memory leak in arduino esp8266 lwip2 or esp8266 sdk or simething else causing oom (might be in rtos sdk too?)
#if 0
	RUN_TEST(
		printf("Test with write after basic\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_send(sock, "X");
			http_socket_recv_flush(sock);
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);
#endif

	// @todo unable to know if the device segfaulted or not
#if 0
	// Ensures there are no memory leaks
	RUN_TEST(
		printf("Test close socket right away\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);

	// Ensure there are no memory leaks for timeouts
	RUN_TEST(
		printf("Test socket timeout\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);
#endif

	return 0;
}
