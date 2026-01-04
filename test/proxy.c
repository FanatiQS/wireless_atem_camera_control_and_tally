#include <stdbool.h> // true
#include <stddef.h> // NULL
#include <stdio.h> // fprintf, stderr, printf
#include <stdlib.h> // abort, getenv
#include <string.h> // strerror
#include <assert.h> // assert

#include <unistd.h> // sleep
#include <spawn.h> // posix_spawn, pid_t
#include <stdlib.h> // setenv
#include <pthread.h> // pthread_create, pthread_t
#include <signal.h> // kill
#include <sys/wait.h> // wait

#include "./utils/utils.h"

// Proxy background process child process id, starts as invalid process id
static pid_t proxy_background_pid = -1;

// Crashes parent process when child exits
static void* proxy_kill_parent(void* arg) {
	(void)arg;

	// Waits for child process to crash
	pid_t result = wait(NULL);
	assert(result == proxy_background_pid);

	// Aborts without triggering test runners abort trap to continue testing
	signal(SIGABRT, SIG_DFL);
	abort();
}

// Cleans up child process when parent aborts
static void proxy_kill_child(int sig) {
	(void)sig;
	assert(proxy_background_pid >= -1);
	if (proxy_background_pid != -1) {
		kill(proxy_background_pid, SIGTERM);
	}
}

int main(void) {
	char* path = getenv("PROXY_PATH");
	char* addr_value = getenv("PROXY_ADDR");

	// Sets up to test proxy running externally
	if (addr_value != NULL) {
		if (path != NULL) {
			fprintf(stderr, "Either PROXY_PATH or PROXY_ADDR should be defined, not both\n");
			abort();
		}
		printf("Testing against proxy on '%s'\n\n", addr_value);
		assert(setenv("ATEM_SERVER_ADDR", addr_value, true) == 0);
	}
	// Sets up to test proxy running in background
	else if (path != NULL) {
		printf("Launching proxy in background thread using '%s'\n\n", path);

		// Runs proxy in background process
		int err = posix_spawn(&proxy_background_pid, path, NULL, NULL, NULL, NULL);
		if (err != 0) {
			fprintf(stderr, "Failed to run background task: %s\n", strerror(err));
			abort();
		}
		assert(setenv("ATEM_SERVER_ADDR", "127.0.0.1", true) == 0);

		// Binds parent process and child process lifetimes
		pthread_t thread;
		assert(pthread_create(&thread, NULL, proxy_kill_parent, NULL) == 0);
		assert(signal(SIGABRT, proxy_kill_child) == 0);
		assert(signal(SIGSEGV, proxy_kill_child) == 0);
		assert(signal(SIGFPE, proxy_kill_child) == 0);

		// Gives proxy time to initialize
		assert(sleep(1) == 0);
	}
	// No way to run tests defined
	else {
		fprintf(stderr, "Missing PROXY_PATH or PROXY_ADDR\n");
		abort();
	}

	// Runs ATEM tests
	atem_server();

	if (path != NULL) {
		assert(proxy_background_pid > 0);
		kill(proxy_background_pid, SIGTERM);
	}
	return runner_exit();
}
