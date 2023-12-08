// Include guard
#ifndef TIMER_H
#define TIMER_H

#include <time.h> // struct timespec

// Default number of milliseconds a timer is allowed to be late for a timing
#define TIMER_LATE (10)

struct timespec timer_create(void);
long timer_get(struct timespec t1);
void timer_get_verify(struct timespec t1, long baseDiff, long lateAllowed);

#endif // TIMER_H
