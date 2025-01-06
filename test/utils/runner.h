// Include guard
#ifndef RUNNER_H
#define RUNNER_H

#include <setjmp.h> // setjmp, jmp_buf
#include <stdbool.h> // bool

extern jmp_buf _runner_jmp;

bool runner_filter(const char* path);
bool _runner_init(const char* path);
void _runner_success(void);
int runner_exit(void);

// Wraps argument into a string
#define WRAP_INNER(arg) #arg
#define WRAP(arg) WRAP_INNER(arg)

// Sets up for running test
#define RUN_TEST() for (\
		bool run = _runner_init(__FILE__ ":" WRAP(__LINE__));\
		run && !setjmp(_runner_jmp);\
		run = false, _runner_success()\
	)

#endif // RUNNER_H
