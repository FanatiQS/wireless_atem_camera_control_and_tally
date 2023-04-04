#include <stdlib.h> // exit, EXIT_FAILURE
#include <stdio.h> // fprintf, stderr

void _testrunner_abort(char* fileName, int lineNumber) {
	fprintf(stderr, "%s:%d\n", fileName, lineNumber);
	exit(EXIT_FAILURE);
}
