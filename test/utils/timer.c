#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <stdio.h> // perror, printf, fprintf, stderr
#include <stdlib.h> // abort

#include "./logs.h" // logs_find
#include "./timer.h"



// Creates a timestamp to measure time delta from
struct timespec timer_create(void) {
	struct timespec timer;
	if (timespec_get(&timer, TIME_UTC) != TIME_UTC) {
		perror("Faield to create timer");
		abort();
	}
	return timer;
}

// Gets time delta from a previous timestamp
long timer_get(struct timespec t1) {
	struct timespec t2 = timer_create();
	long diff = ((t2.tv_sec - t1.tv_sec) * 1000) + ((t2.tv_nsec - t1.tv_nsec) / 1000000);

	if (logs_find("timer")) {
		printf("Timer difference: %zu\n", diff);
	}

	return diff;
}

// Ensures time delta from a previous timestamp is baseDiff ms and not more than lateAllowed ms late
void timer_get_verify(struct timespec t1, long baseDiff, long lateAllowed) {
	long diff = timer_get(t1);
	if ((diff < baseDiff) || (diff > (baseDiff + lateAllowed))) {
		fprintf(stderr,
			"Expected timer delta between %zu and %zu, but got %zu\n",
			baseDiff, baseDiff + lateAllowed, diff
		);
		abort();
	}
}
