#ifndef RUNNER_H
#define RUNNER_H

void _testrunner_abort(char* fileName, int lineNumber);
#define testrunner_abort() _testrunner_abort(__FILE__, __LINE__)

#endif // RUNNER_H
