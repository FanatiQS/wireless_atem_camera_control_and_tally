// Include guard
#ifndef TIMEDIFF_H
#define TIMEDIFF_H

#include <time.h> // struct timespec

// Default number of milliseconds a timer is allowed to be late for a timing
#define TIMEDIFF_LATE (10)

struct timespec timediff_mark(void);
int timediff_get(struct timespec mark_start);
void timediff_get_verify(struct timespec mark_start, int diff_base, int late_allowed);

#endif // TIMEDIFF_H
