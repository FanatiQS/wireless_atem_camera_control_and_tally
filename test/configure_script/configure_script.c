#include <stdbool.h> // bool, true
#include <stdio.h> // perror, FILE, popen, fread
#include <stddef.h> // NULL
#include <assert.h> // assert
#include <stdlib.h> // abort
#include <string.h> // strlen, strcat

#include <unistd.h> // close, usleep
#include <sys/socket.h> // SOCK_STREAM, setsockopt, SOL_SOCKET, SO_REUSEADDR, listen, accept

#include "../utils/utils.h"

#define BUF_LEN (1024)

// Ensures that request sent through configuration script is received correctly
static void config_script_test(const char* request, const char* request_expected) {
	// Creates HTTP server listen socket that does not hold port after close (lingering)
	int sock_listen = simple_socket_create(SOCK_STREAM);
	if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) != 0) {
		close(sock_listen);
		perror("Failed to set SO_REUSEADDR flag");
		abort();
	}
	simple_socket_listen(sock_listen, 80);

	// Sets HTTP server to listen for connections
	int listen_result = listen(sock_listen, 10);
	if (listen_result == -1) {
		close(sock_listen);
		perror("Failed to listen for HTTP clients");
		abort();
	}
	assert(listen_result == 0);

	// Makes HTTP request with configuration script
	char cmd_buf[BUF_LEN] = "../tools/configure.sh 127.0.0.1 ";
	assert((strlen(cmd_buf) + strlen(request)) < sizeof(cmd_buf));
	strcat(cmd_buf, request);
	FILE* cmd = popen(cmd_buf, "r");

	// Accepts HTTP client from configuration script
	int sock_accepted = accept(sock_listen, NULL, NULL);
	close(sock_listen);
	if (sock_accepted == -1) {
		perror("Failed to accept HTTP client");
		abort();
	}

	// Ensures request is correct
	http_socket_recv_cmp(sock_accepted, "POST / HTTP/1.1\r\n");
	http_socket_recv_until(sock_accepted, "\r\n\r\n");
	http_socket_recv_cmp(sock_accepted, request_expected);

	// Closes HTTP communication correctly for curl to be happy
	http_socket_send_string(sock_accepted, "HTTP/1.1 200 OK\r\n\r\n");
	assert(fread(NULL, 0, 0, cmd) == 0);
	http_socket_close(sock_accepted);

	// Delay to allow listening port to become available
	usleep(2000);
}

int main(void) {
	// Ensures basic arguments work
	RUN_TEST() {
		config_script_test("banana=123", "banana=123");
	}

	// Ensures quoted spaces work when quoting value only
	RUN_TEST() {
		config_script_test("banana=\"123 456\"", "banana=123%20456");
	}

	// Ensures quoting spaces work when quoting key and value
	RUN_TEST() {
		config_script_test("\"banana=123 456\"", "banana=123%20456");
	}

	// Ensures escaped spaces work
	RUN_TEST() {
		config_script_test("banana=123\\ 456", "banana=123%20456");
	}

	// Ensures equal signs after delimiter is treaded like normal
	RUN_TEST() {
		config_script_test("banana=123=456", "banana=123%3D456");
	}

	// Ensures string can not be escaped
	RUN_TEST() {
		config_script_test("banana=\"123;ls\"", "banana=123%3Bls");
	}

	return runner_exit();
}
