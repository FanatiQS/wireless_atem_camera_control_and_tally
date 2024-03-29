// Include guard
#ifndef TIMEDIFF_H
#define TIMEDIFF_H

#include <time.h> // struct timespec

// Default number of milliseconds a timer is allowed to be late for a timing
#define TIMEDIFF_LATE (10)

struct timespec timediff_mark(void);
long timediff_get(struct timespec startMark);
void timediff_get_verify(struct timespec startMark, long baseDiff, long lateAllowed);

#endif // TIMEDIFF_H
