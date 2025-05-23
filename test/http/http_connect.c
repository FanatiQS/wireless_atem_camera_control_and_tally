#include <stdio.h> // printf, fprintf, stderr
#include <stdlib.h> // getenv, atoi, malloc, free, abort
#include <errno.h> // ECONNRESET
#include <stdbool.h> // bool
#include <time.h> // time, NULL

#include "./http_sock.h" // http_socket_create, http_socket_send_string, http_socket_recv_len, http_socket_close, http_socket_recv_flush, http_socket_recv_cmp_status, http_socket_recv_error
#include "../utils/runner.h" // RUN_TEST, runner_exit

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
	if (iters <= 0) {
		fprintf(stderr, "Invalid HTTP_CONNECTION_ITERS value: %s\n", itersEnvStr);
		abort();
	}

	// Ensure there are no memory leaks for normal HTTP requests
	RUN_TEST() {
		printf("Test valid requests\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GET / HTTP/1.1\r\n\r\n");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}

	// Ensures there are no memory leaks for invalid HTTP request
	RUN_TEST() {
		printf("Test invalid requests\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_recv_cmp_status(sock, 405);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}

	// Ensures there are no memory leaks when sending after server close
	RUN_TEST() {
		printf("Test write after valid request\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GET / HTTP/1.1\r\n\r\n");
			http_socket_send_string(sock, "X");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}

	// @todo memory leak in arduino esp8266 lwip2 (not lwip1.4) causing oom (might be in rtos sdk too?)
#if 0
	// Ensures there are no memory leaks when sending after server close on invalid request
	RUN_TEST() {
		printf("Test write after invalid request\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_send_string(sock, "X");
			http_socket_recv_cmp_status(sock, 405);
			http_socket_recv_flush(sock);
			http_socket_recv_error(sock, ECONNRESET);
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}
#endif

	// Ensures there are no memory leaks when closing from client
	RUN_TEST() {
		printf("Test close socket right away\n");
		for (int i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}

	// @todo ESP8266 Arduino segfaults (only lwip2), causing closing to fail with connection reset by peer
	// Ensures max number of simultaneous connections does not cause memory segfaults
	RUN_TEST() {
		printf("Test simultaneous sockets\n");
		int* socks = malloc(sizeof(int) * (unsigned int)iters);

		// Creates client connections to server
		for (int i = 0; i < iters; i++) {
			socks[i] = http_socket_create();
			progressprint(i, iters);
		}

		// Closes all sockets
		for (int i = 0; i < iters; i++) {
			http_socket_close(socks[i]);
		}
		free(socks);
	}

	// Ensure there are no memory leaks for timeouts
	RUN_TEST() {
		printf("Test socket timeout\n");
		for (int i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			progressprint(i, iters);
		}
	}

	return runner_exit();
}
