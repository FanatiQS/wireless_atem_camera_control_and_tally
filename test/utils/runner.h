#ifndef RUNNER_H
#define RUNNER_H

void _testrunner_abort(char* fileName, int lineNumber);
#define testrunner_abort() _testrunner_abort(__FILE__, __LINE__)

void _testrunner_run(char* name, void(*callback)(void));
#define TESTRUNNER(fn) _testrunner_run(#fn, fn)

#endif // RUNNER_H
