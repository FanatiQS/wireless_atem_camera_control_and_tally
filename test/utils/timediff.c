#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <stdio.h> // perror, printf, fprintf, stderr
#include <stdlib.h> // abort

#include "./logs.h" // logs_find
#include "./timediff.h"



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
long timediff_get(struct timespec startMark) {
	struct timespec endMark = timediff_mark();
	long diff = ((endMark.tv_sec - startMark.tv_sec) * 1000) + ((endMark.tv_nsec - startMark.tv_nsec) / 1000000);

	if (logs_find("timer")) {
		printf("Timer difference: %lu\n", diff);
	}

	return diff;
}

// Ensures time delta from a previous timestamp is baseDiff ms and not more than lateAllowed ms late
void timediff_get_verify(struct timespec startMark, long baseDiff, long lateAllowed) {
	long diff = timediff_get(startMark);
	if ((diff < baseDiff) || (diff > (baseDiff + lateAllowed))) {
		fprintf(stderr,
			"Expected timer delta between %lu and %lu, but got %lu\n",
			baseDiff, baseDiff + lateAllowed, diff
		);
		abort();
	}
}
