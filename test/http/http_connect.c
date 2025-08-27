#include <stdio.h> // printf, fprintf, stderr
#include <stdlib.h> // getenv, atoi, malloc, free, abort
#include <errno.h> // ECONNRESET
#include <stdbool.h> // bool
#include <time.h> // time, NULL

#include "./http_sock.h" // http_socket_create, http_socket_send_string, http_socket_recv_len, http_socket_close, http_socket_recv_flush, http_socket_recv_cmp_status, http_socket_recv_error
#include "../utils/runner.h" // RUN_TEST, runner_exit
#include "../utils/logs.h" // logs_print_progress

int main(void) {
	// Number of iterations to trigger memory leak if available
	char* iters_str = getenv("HTTP_CONNECTION_ITERS");
	size_t iters = (iters_str) ? (size_t)atoi(iters_str) : 1000;
	if ((int)iters <= 0) {
		fprintf(stderr, "Invalid HTTP_CONNECTION_ITERS value: %s\n", iters_str);
		abort();
	}

	// Ensure there are no memory leaks for normal HTTP requests
	RUN_TEST() {
		printf("Test valid requests\n");
		for (size_t i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GET / HTTP/1.1\r\n\r\n");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}

	// Ensures there are no memory leaks for invalid HTTP request
	RUN_TEST() {
		printf("Test invalid requests\n");
		for (size_t i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_recv_cmp_status(sock, 405);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}

	// Ensures there are no memory leaks when sending after server close
	RUN_TEST() {
		printf("Test write after valid request\n");
		for (size_t i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GET / HTTP/1.1\r\n\r\n");
			http_socket_send_string(sock, "X");
			http_socket_recv_cmp_status(sock, 200);
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}

	// @todo memory leak in arduino esp8266 lwip2 (not lwip1.4) causing oom (might be in rtos sdk too?)
#if 0
	// Ensures there are no memory leaks when sending after server close on invalid request
	RUN_TEST() {
		printf("Test write after invalid request\n");
		for (size_t i = 1; i <= iters; i++) {
			int sock = http_socket_create();
			http_socket_send_string(sock, "GEF / HTTP/1.1\r\n\r\n");
			http_socket_send_string(sock, "X");
			http_socket_recv_cmp_status(sock, 405);
			http_socket_recv_flush(sock);
			http_socket_recv_error(sock, ECONNRESET);
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}
#endif

	// Ensures there are no memory leaks when closing from client
	RUN_TEST() {
		printf("Test close socket right away\n");
		for (size_t i = 0; i < iters; i++) {
			int sock = http_socket_create();
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}

	// @todo ESP8266 Arduino segfaults (only lwip2), causing closing to fail with connection reset by peer
	// Ensures max number of simultaneous connections does not cause memory segfaults
	RUN_TEST() {
		printf("Test simultaneous sockets\n");
		int* socks = malloc(sizeof(int) * (unsigned int)iters);

		// Creates client connections to server
		for (size_t i = 0; i < iters; i++) {
			socks[i] = http_socket_create();
			logs_print_progress(i, iters);
		}

		// Closes all sockets
		for (size_t i = 0; i < iters; i++) {
			http_socket_close(socks[i]);
		}
		free(socks);
	}

	// Ensure there are no memory leaks for timeouts
	RUN_TEST() {
		printf("Test socket timeout\n");
		for (size_t i = 0; i < iters; i++) {
			int sock = http_socket_create();
			while (http_socket_recv_len(sock));
			http_socket_close(sock);
			logs_print_progress(i, iters);
		}
	}

	return runner_exit();
}
