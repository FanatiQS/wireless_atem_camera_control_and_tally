// Include guard
#ifndef RUNNER_H
#define RUNNER_H

#include <setjmp.h> // setjmp, jmp_buf
#include <stdio.h> // printf
#include <stdbool.h> // bool

extern jmp_buf _runner_abort_jmp;
bool _runner_mode_abort(const char* file, int line);

// Runs a block of test code
#define RUN_TEST(...)\
	do {\
		if (_runner_mode_abort(__FILE__, __LINE__) || !setjmp(_runner_abort_jmp)) {\
			__VA_ARGS__;\
			printf("Test successful\n\n");\
		}\
	} while (0)

int runner_exit(void);

#endif // RUNNER_H
