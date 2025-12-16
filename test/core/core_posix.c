#include <assert.h> // assert
#include <errno.h> // errno, EFAULT, EBADF
#include <stdbool.h> // true, false
#include <stdint.h> // uint8_t
#include <stddef.h> // NULL

#include <arpa/inet.h> // htonl
#include <netinet/in.h> // INADDR_LOOPBACK, in_addr_t
#include <sys/socket.h> // socklen_t, struct sockaddr, getsockname, connect
#include <pthread.h> // pthread_create
#include <stdlib.h> // setenv

#include "../utils/utils.h"

// Runs ATEM client, reinitializes when test failure in main thread nukes all file descriptors
static void* atem_posix_thread(void* arg) {
	(void)arg;
	struct atem_posix_ctx ctx;
	while (true) {
		assert(atem_init(&ctx, htonl(INADDR_LOOPBACK)));
		assert(atem_send(&ctx));
		while (atem_next(&ctx) != ATEM_POSIX_STATUS_ERROR_SYSTEM || errno != EBADF);
	}
}

int main(void) {
	// Ensures valid ip address argument is accepted
	RUN_TEST() {
		struct atem_posix_ctx posix_client;
		assert(atem_init(&posix_client, INADDR_LOOPBACK) == true);
	}

	// Ensures invalid ip address argument is rejected with error code
	RUN_TEST() {
		struct atem_posix_ctx posix_client;
		assert(atem_init(&posix_client, (in_addr_t)-1) == false);
		assert(errno == EFAULT);
	}

	// Ensures too short payload fails to parse
	RUN_TEST() {
		// Initialises ATEM server
		int server_sock = atem_socket_create();
		simple_socket_listen(server_sock, ATEM_PORT);

		// Initializes ATEM posix client
		struct atem_posix_ctx posix_client;
		assert(atem_init(&posix_client, htonl(INADDR_LOOPBACK)));

		// Connects server to client
		struct sockaddr addr;
		socklen_t addrlen = sizeof(addr);
		assert(getsockname(posix_client.sock, &addr, &addrlen) == 0);
		assert(connect(server_sock, &addr, addrlen) == 0);

		// Sends too short packet
		uint8_t buf[ATEM_LEN_HEADER - 1] = {0};
		simple_socket_send(server_sock, buf, sizeof(buf));

		// Expects parse to fail
		assert(atem_recv(&posix_client) == ATEM_POSIX_STATUS_ERROR_PARSE);

		atem_socket_close(server_sock);
	}

	// Ensures timing out returns correct status enum
	RUN_TEST() {
		struct atem_posix_ctx posix_client;
		assert(atem_init(&posix_client, htonl(INADDR_LOOPBACK)));
		assert(atem_poll(&posix_client) == ATEM_POSIX_STATUS_DROPPED);
	}

	// Runs ATEM client tests against POSIX client running in background thread
	pthread_t pid;
	assert(pthread_create(&pid, NULL, atem_posix_thread, NULL) == 0);
	setenv("ATEM_CLIENT_ADDR", "127.0.0.1", true);
	atem_client();

	return runner_exit();
}
