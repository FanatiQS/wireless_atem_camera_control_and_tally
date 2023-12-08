#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <stdio.h> // perror, printf, fprintf, stderr
#include <stdlib.h> // abort

#include "./logs.h" // logs_find
#include "./timer.h"



// Creates a timestamp to measure time delta from
struct timespec timediff_mark(void) {
	struct timespec marker;
	if (timespec_get(&marker, TIME_UTC) != TIME_UTC) {
		perror("Faield to create timer");
		abort();
	}
	return marker;
}

// Gets time delta from a previous timestamp
long timediff_get(struct timespec t1) {
	struct timespec t2 = timediff_mark();
	long diff = ((t2.tv_sec - t1.tv_sec) * 1000) + ((t2.tv_nsec - t1.tv_nsec) / 1000000);

	if (logs_find("timer")) {
		printf("Timer difference: %lu\n", diff);
	}

	return diff;
}

// Ensures time delta from a previous timestamp is baseDiff ms and not more than lateAllowed ms late
void timediff_get_verify(struct timespec t1, long baseDiff, long lateAllowed) {
	long diff = timediff_get(t1);
	if ((diff < baseDiff) || (diff > (baseDiff + lateAllowed))) {
		fprintf(stderr,
			"Expected timer delta between %lu and %lu, but got %lu\n",
			baseDiff, baseDiff + lateAllowed, diff
		);
		abort();
	}
}
