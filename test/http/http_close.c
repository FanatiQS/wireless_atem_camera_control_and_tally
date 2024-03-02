#include <sys/socket.h> // shutdown, SHUT_RDWR

#include "../utils/utils.h"

// Makes an HTTP request but closes it right away
void test_close(const char* req) {
	int sock = http_socket_create();
	http_socket_send_string(sock, req);
	shutdown(sock, SHUT_RDWR);
	while (http_socket_recv_len(sock));
	http_socket_close(sock);
}

int main(void) {
	// Closes unsupported/invalid method
	RUN_TEST() {
		test_close("PUT /");
	}

	// Closes incomplete GET request
	RUN_TEST() {
		test_close("GET ");
	}

	// Closes GET request
	RUN_TEST() {
		test_close("GET /");
	}

	// Closes incomplete POST request
	RUN_TEST() {
		test_close("POST");
	}
	RUN_TEST() {
		test_close("POST /");
	}

	// Closes invalid POST request without length
	RUN_TEST() {
		test_close("POST / HTTP/1.1\r\n\r\n");
	}

	// Successful POST request closed after configuration commit
	RUN_TEST() {
		test_close("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n");
	}

	return runner_exit();
}
