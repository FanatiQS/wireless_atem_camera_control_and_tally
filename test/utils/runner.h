// Include guard
#ifndef RUNNER_H
#define RUNNER_H

#include <setjmp.h> // setjmp, jmp_buf
#include <stdbool.h> // bool

extern jmp_buf runner_jmp;

bool runner_filter(const char* path);
bool runner_start(const char* path);
void runner_success(void);
int runner_exit(void);

// Wraps argument into a string
#define WRAP_INNER(arg) #arg
#define WRAP(arg) WRAP_INNER(arg)

// Sets up for running test
#define RUN_TEST() for ( \
		bool run = runner_start(__FILE__ ":" WRAP(__LINE__)); \
		run && !setjmp(runner_jmp); \
		run = false, runner_success() \
	)

#endif // RUNNER_H
