#include <stdio.h> // printf, perror
#include <stdlib.h> // getenv, atoi, abort
#include <errno.h> // errno, ECONNRESET
#include <assert.h> // assert
#include <stdbool.h> // bool
#include <time.h> // time, NULL

#include <unistd.h> // usleep
#include <sys/select.h> // select, FD_SETSIZE, FD_SET, FD_ZERO, fd_set, FD_ISSET
#include <sys/time.h> // struct timeval

#include "./http_sock.h" // http_socket_create, http_socket_send, http_socket_recv_len, http_socket_close, http_socket_recv_flush, http_socket_recv_cmp_status, http_socket_recv_close
#include "../utils/runner.h" // RUN_TEST

// Roughly number of seconds between progress prints
#define PROGRESS_PRINT_TIMER 8

// Prints progress into test until finished
static void progressprint(int progress, int finish) {
	static bool cmp;
	if (!(time(NULL) % PROGRESS_PRINT_TIMER) != cmp) return;
	cmp = !cmp;
	if (cmp) return;
	printf("progress: %d/%d\n", progress, finish);
}



int main(void) {
	// Number of iterations to trigger memory leak if available
	char* itersEnvStr = getenv("HTTP_CONNECTION_ITERS");
	int iters = (itersEnvStr) ? atoi(itersEnvStr) : 1000;

	// Ensure there are no memory leaks for normal HTTP requests
	RUN_TEST(
		printf("Test valid requests\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GET / HTTP/1.0\r\n\r\n");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	);

	// Ensures there are no memory leaks for invalid HTTP request
	RUN_TEST(
		printf("Test invalid requests\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GEF / HTTP/1.0\r\n\r\n");
			http_socket_recv_cmp_status(sock, 405);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	);

	// Ensures there are no memory leaks when sending after server close
	RUN_TEST(
		printf("Test write after valid request\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GET / HTTP/1.0\r\n\r\n");
			http_socket_send(sock, "X");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	);

	// @todo memory leak in arduino esp8266 lwip2 (not lwip1.4) causing oom (might be in rtos sdk too?)
#if 0
	// Ensures there are no memory leaks when sending after server close on invalid request
	RUN_TEST(
		printf("Test write after invalid request\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_send(sock, "X");
			http_socket_recv_cmp_status(sock, 405);
			http_socket_recv_flush(sock);
			http_socket_recv_error(sock);
			if (errno != ECONNRESET) {
				perror("Unexpected error");
				abort();
			}
			http_socket_close(sock);
			if (!(i % 100)) printf("%d\n", i);
		}
	);
#endif

	// Ensures there are no memory leaks when closing from client
	RUN_TEST(
		printf("Test close socket right away\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			usleep(20000);
			http_socket_close(sock);
			progressprint(i, iters);
		}
	);

	// @todo memory issue in arduino esp8266 lwip2 (not arduino lwip1.4 or esp8266 rtos sdk)
#if 1
	// Ensures max number of simultanious connections does not cause memory segfaults
	RUN_TEST(
		printf("Test simultanious sockets\n");
		int socks[iters];
		int nfds = 0;
		fd_set fds;
		FD_ZERO(&fds);
		for (int i = 0; i < iters; i++) {
			// Creates client connected to server
			int sock = http_socket_create();
			progressprint(i, iters);

			// Ensures existing clients were not dropped incorrectly
			socks[i] = sock;
			nfds = (nfds > sock) ? (sock + 1) : nfds;
			FD_SET(i, &fds);
			fd_set fdsCopy = fds;
			struct timeval tv = {0};
			assert(select(nfds, &fdsCopy, NULL, NULL, &tv) != -1);
			for (int j = 0; j <= i; j++) {
				if (FD_ISSET(socks[j], &fdsCopy)) {
					http_socket_recv_close(socks[j]);
				}
			}
		}
		for (int i = 0; i < iters; i++) {
			http_socket_close(socks[i]);
		}
	);
#endif

	// Ensure there are no memory leaks for timeouts
	RUN_TEST(
		printf("Test socket timeout\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	);

	return 0;
}