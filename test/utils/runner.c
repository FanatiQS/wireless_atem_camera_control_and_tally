#include <setjmp.h> // longjmp, jmp_buf
#include <signal.h> // signal, SIGABRT
#include <stdio.h> // printf, fprintf, stderr
#include <stdlib.h> // abort, getenv, EXIT_SUCCESS
#include <string.h> // strcmp
#include <stddef.h> // NULL
#include <stdbool.h> // bool, true

#include <unistd.h> // close, sleep

#include "./simple_socket.h" // SIMPLE_SOCKET_MAX_FD
#include "../../core/atem.h" // ATEM_TIMEOUT
#include "./runner.h"

static int runner_all = 0;
static int runner_fails = 0;

jmp_buf _runner_jmp;

// Trap callback to exit failing test and continue with next
static void runner_fail(int sig) {
	// Prevents unused argument compiler warning
	(void)sig;

	// Document failure
	printf("Test failed\n\n");
	runner_fails++;

	// Cleans up ALL open file descriptors other than stdin/stdout/stderr
	for (int fd = 3; fd < SIMPLE_SOCKET_MAX_FD; fd++) {
		close(fd);
	}
	sleep(ATEM_TIMEOUT);

	// Jumps back out and escapes RUN_TEST call
	longjmp(_runner_jmp, 0);
}

// Sets up for a new test to run
bool _runner_init(const char* file, int line) {
	// Ignores all tests not matching line number if environment variable is defined
	char* singleTest = getenv("RUNNER_SINGLE");
	if (singleTest && atoi(singleTest) != line) {
		return false;
	}

	// Document test started
	printf("Test started: %s:%d\n", file, line);
	runner_all++;

	// Sets test to abort on failure
	char* mode = getenv("RUNNER_MODE");
	if (mode == NULL || !strcmp(mode, "abort")) {
		return true;
	}

	// Invalid test mode value
	if (strcmp(mode, "all")) {
		fprintf(stderr, "Invalid RUNNER_MODE value: %s\n", mode);
		abort();
	}

	// Sets test to continue with remaining tests one failure
	signal(SIGABRT, runner_fail);
	return true;
}

// Runs when a test runs successfully without any errors
void _runner_success(void) {
	printf("Test successful\n\n");
}

// Prints and returns number of failed tests if not running in abort mode
int runner_exit(void) {
	char* mode = getenv("RUNNER_MODE");
	if (mode == NULL || !strcmp(mode, "abort")) {
		return EXIT_SUCCESS;
	}

	printf("Tests failed: %d/%d\n", runner_fails, runner_all);
	return runner_fails;
}
