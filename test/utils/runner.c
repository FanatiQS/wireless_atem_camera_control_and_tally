#include <setjmp.h> // longjmp, jmp_buf
#include <signal.h> // signal, SIGABRT
#include <stdio.h> // printf, fprintf, stderr, setvbuf, _IOLBF
#include <stdlib.h> // abort, getenv, EXIT_SUCCESS, atoi
#include <string.h> // strcmp
#include <stddef.h> // NULL
#include <stdbool.h> // bool, true, false
#include <assert.h> // assert

#include <unistd.h> // close, sleep

#include "./simple_socket.h" // SIMPLE_SOCKET_MAX_FD
#include "../../core/atem.h" // ATEM_TIMEOUT
#include "./runner.h"

static int runner_all = 0;
static int runner_fails = 0;
static int runner_skipped = 0;

jmp_buf runner_jmp;

// Trap callback to exit failing test and continue with next
#if __STDC_VERSION__ >= 202311 // >= C23
[[noreturn]]
#elif __STDC_VERSION__ >= 201112 // C11
_Noreturn
#endif // __STDC__VERSION
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
	longjmp(runner_jmp, 0);
}

// Ensures path matches all required patterns
bool runner_filter(const char* path) {
	const char delimiter = ',';

	// Gets pattern filters
	const char* filter = getenv("RUNNER_FILTER");
	if (filter == NULL) {
		return true;
	}

	bool filtered = false;
	bool matched = false;
	while (*filter != '\0') {
		// Checks if pattern is inclusive or exclusive
		bool exclude = *filter == '-';
		if (exclude) {
			filter++;
		}
		// Flushes when there has already been a successful inclusive match since all exclusive patterns has to pass
		else if (matched) {
			while (*filter != delimiter && *filter != '\0') {
				filter++;
			}
			if (*filter == '\0') {
				break;
			}
			filter++;
			continue;
		}
		// Filters if inclusive match is provided
		else {
			filtered = true;
		}

		// Checks if path matches pattern
		const char* match = path;
		const char* wildcard = NULL;
		while (*match != '\0') {
			// Moves to next character in pattern on character match
			if (*filter == *match) {
				filter++;
			}
			// Wildcard character sets checkpoint to restart
			else if (*filter == '*') {
				filter++;
				wildcard = filter;
			}
			// Fails pattern match if no wildcard checkpoint has been set
			else if (wildcard == NULL) {
				break;
			}
			// Resets back to wildcard checkpoint on character mismatch
			else {
				filter = wildcard;
			}
			match++;
		}

		// Handles successful pattern matching
		if (*filter == delimiter || *filter == '\0') {
			if (exclude) {
				return false;
			}
			matched = true;
		}

		// Flushes remaining pattern string
		while (*filter != delimiter && *filter != '\0') {
			filter++;
		}
		if (*filter == delimiter) {
			filter++;
		}
	}

	// Succeeds for matched inclusive pattern or no inclusive patterns present
	return !filtered || matched;
}

// Sets up for a new test to run
bool runner_start(const char* path) {
	// Line buffers both stdout and stderr to fix out of order logs when stdout is fully buffered
	bool buffer_mode_initialized = false;
	if (!buffer_mode_initialized) {
		setvbuf(stderr, NULL, _IOLBF, 0x1000);
		buffer_mode_initialized = true;
	}

	// Ignores all tests not matching filter pattern
	if (!runner_filter(path)) {
		printf("Test skipped: %s\n", path);
		runner_skipped++;
		return false;
	}

	// Document test started
	printf("Test started: %s\n", path);
	runner_all++;

	// Sets test to abort on failure
	char* mode = getenv("RUNNER_MODE");
	if (mode == NULL || strcmp(mode, "abort") == 0) {
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
void runner_success(void) {
	printf("Test successful\n\n");
}

// Prints and returns number of failed tests if not running in abort mode
int runner_exit(void) {
	char* mode = getenv("RUNNER_MODE");
	if (mode == NULL || strcmp(mode, "abort") == 0) {
		assert(runner_fails == 0);
		printf("Number of tests completed: %d (%d skipped)\n", runner_all, runner_skipped);
		return EXIT_SUCCESS;
	}

	assert(strcmp(mode, "all") == 0);
	printf("Tests failed: %d/%d (%d skipped)\n", runner_fails, runner_all, runner_skipped);
	return runner_fails;
}
