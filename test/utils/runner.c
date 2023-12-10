#include <setjmp.h> // longjmp, jmp_buf
#include <signal.h> // signal, SIGABRT
#include <stdio.h> // printf, fprintf, stderr
#include <stdlib.h> // abort, getenv, EXIT_SUCCESS
#include <string.h> // strcmp
#include <stddef.h> // NULL
#include <stdbool.h> // bool, true, false
#include <limits.h> // INT_MAX

#include <unistd.h> // close

#include "./simple_socket.h" // SIMPLE_SOCKET_MAX_FD
#include "../../src/atem.h" // ATEM_TIMEOUT
#include "./runner.h"

static int runner_all = 0;
static int runner_fails = 0;

jmp_buf _runner_abort_jmp;

// Trap callback to exit test failing test and continue with next
static void runner_abort_trap(int signal) {
	// Prevents unused argument compiler warning
	(void)signal;

	// Document failure
	printf("Test failed\n\n");
	runner_fails++;

	// Cleans up ALL open file descriptors other than stdin/stdout/stderr
	for (int fd = 3; fd < SIMPLE_SOCKET_MAX_FD; fd++) {
		close(fd);
	}
	sleep(ATEM_TIMEOUT);

	// Jumps back out and escapes RUN_TEST call
	longjmp(_runner_abort_jmp, 0);
}

// Sets up for test and allows RUN_TEST to setjmp by returning false
bool _runner_mode_abort(const char* file, int line) {
	// Document test started
	printf("Test started: %s:%d\n", file, line);
	runner_all++;

	// Allows continuing with remaining when test fails
	char* mode = getenv("RUNNER_MODE");
	if (mode == NULL || !strcmp(mode, "all")) {
		signal(SIGABRT, runner_abort_trap);
		return false;
	}

	// Aborts on test failure
	if (!strcmp(mode, "abort")) {
		return true;
	}

	// Invalid test mode value
	fprintf(stderr, "Invalid RUNNER_MODE value: %s\n", mode);
	abort();
}

// Prints and returns number of failed tests if not running in abort mode
int runner_exit(void) {
	char* mode = getenv("RUNNER_MODE");
	if (mode != NULL && !strcmp(mode, "abort")) {
		return EXIT_SUCCESS;
	}

	printf("Tests failed: %d/%d\n", runner_fails, runner_all);
	return runner_fails;
}
