#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <stdio.h> // perror
#include <ctype.h> // isdigit
#include <assert.h> // assert
#include <stdint.h> // uint16_t
#include <stdbool.h> // true

#include <getopt.h> // getopt, optarg

#include "./atem_server.h" // atem_server_init
#include "./atem_cache.h" // atem_cache_init
#include "./atem_assert.h" // atem_assert
#include "./timeout.h" // timeout_get, timeout_dispatch

#include <poll.h> // poll, struct pollfd, POLLIN

// Gets uint16_t from command line argument option
static uint16_t cli_option_get(void) {
	assert(optarg != NULL);
	uint16_t value = 0;
	int index = 0;
	while (isdigit(optarg[index])) {
		if (value > (UINT16_MAX / 10)) return 0;
		value *= 10;
		int num = optarg[index] - '0';
		if (value > (UINT16_MAX - num)) return 0;
		value += num;
		index++;
	}
	return (optarg[index] == '\0') ? value : 0;
}

int main(int argc, char** argv) {
	// Sets ATEM proxy server configuration
	int opt;
	while ((opt = getopt(argc, argv, "hl:r:p:")) != -1) switch (opt) {
		case 'l': {
			atem_server.sessions_limit = cli_option_get();
			if (atem_server.sessions_limit == 0) {
				printf("Invalid sessions limit: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'r': {
			atem_server.retransmit_delay = cli_option_get();
			if (atem_server.retransmit_delay == 0) {
				printf("Invalid retransmit delay: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'p': {
			atem_server.ping_interval = cli_option_get();
			if (atem_server.ping_interval == 0) {
				printf("Invalid ping interval: %s\n", optarg);
				return EXIT_FAILURE;
			}
			break;
		}
		case 'h': {
			printf(
				"Usage: %s [options] ...\n"
				"Options:\n"
				"\t-l <arg>        Limit the number of concurrent sessions to <arg>. Defaults to 5.\n"
				"\t-r <arg>        Time in ms before an unacknowledged packet is retransmitted. Defaults to 200ms.\n"
				"\t-p <arg>        Time in ms between pings. Defaults to 500ms.\n",
				argv[0]
			);
			return EXIT_SUCCESS;
		}
		default: {
			return EXIT_FAILURE;
		}
	}

	// Initializes the ATEM cache
	atem_cache_init(8);

	// Initializes ATEM proxy server
	if (!atem_server_init()) {
		perror("Failed to create socket");
		return EXIT_FAILURE;
	}

	// Runs ATEM proxy server event loop
	struct pollfd pollfd = { .fd = atem_server.sock, .events = POLLIN };
	while (true) {
		int poll_len = poll(&pollfd, 1, timeout_get());
		if (poll_len == -1) {
			perror("Failed to poll ATEM server socket");
			return EXIT_FAILURE;
		}
		assert(poll_len >= 0);
		assert(poll_len <= 1);

		if (pollfd.revents) {
			atem_server_recv();
		}
	}

	return EXIT_SUCCESS;
}
