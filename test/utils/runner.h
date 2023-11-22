#ifndef RUNNER_H
#define RUNNER_H

void _testrunner_abort(char* fileName, int lineNumber);
#define testrunner_abort() _testrunner_abort(__FILE__, __LINE__)

void _testrunner_run(char* name, void(*callback)(void));
#define TESTRUNNER(fn) _testrunner_run(#fn, fn)

#include <stdio.h> // printf, fprintf, stderr

#define RUN_TEST(codeBlock)\
	do {\
		printf("Test started %s:%d\n", __FILE__, __LINE__);\
		codeBlock;\
		printf("Test completed\n\n");\
	} while(0)

#endif // RUNNER_H
