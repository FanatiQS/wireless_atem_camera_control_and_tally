// Include guard
#ifndef RUNNER_H
#define RUNNER_H

#include <setjmp.h> // setjmp, jmp_buf
#include <stdbool.h> // bool

extern jmp_buf _runner_jmp;
bool _runner_init(const char* file, int line);
void _runner_success(void);
int runner_exit(void);

// Sets up to running test
#define _RUN_TEST() for (\
		bool run = _runner_init(__FILE__, __LINE__);\
		run && !setjmp(_runner_jmp);\
		run = false, _runner_success()\
	)

// Runs a block of test code
#define RUN_TEST(...) _RUN_TEST() {__VA_ARGS__}


#endif // RUNNER_H
