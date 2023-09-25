#include <unistd.h> // sleep
#include <assert.h> // assert
#include <stdlib.h> // getenv

#include "./http_sock.h" // http_socket_create, http_socket_await_close, http_socket_close



// Closes the socket after creating it
void http_accept_close(char* addr) {
	int sock = http_socket_create(addr);
	http_socket_close(sock);
}

// Awaits server to close the socket
void http_accept_drop(char* addr) {
	int sock = http_socket_create(addr);
	http_socket_await_close(sock);
	http_socket_close(sock);
}

// Awaits server to request close and refuse to answer
void http_accept_refuse_close_answer(char* addr) {
	int sock = http_socket_create(addr);
	http_socket_await_close(sock);
	sleep(20);
}



int main(int argc, char** argv) {
	// Gets device address from environment variable
	char* addr = getenv("DEVICE_ADDR");
	assert(addr);

	// Runs tests
	http_accept_drop(addr);
	http_accept_close(addr);
	http_accept_refuse_close_answer(addr);

	return 0;
}
