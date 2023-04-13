#include <stdlib.h> // exit, EXIT_FAILURE
#include <stdio.h> // fprintf, stderr

#include "./logs.h" // print_debug

void _testrunner_abort(char* fileName, int lineNumber) {
	print_debug("%s:%d\n", fileName, lineNumber);
	exit(EXIT_FAILURE);
}

void _testrunner_run(char* name, void(*callback)(void)) {
	print_debug("Starting test: %s\n", name);
	callback();
	print_debug("Test finished: %s\n", name);
}
