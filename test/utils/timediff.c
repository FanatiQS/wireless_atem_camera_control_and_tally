#include <time.h> // struct timespec, timespec_get, TIME_UTC
#include <stdio.h> // perror, printf, fprintf, stderr
#include <stdlib.h> // abort
#include <assert.h> // assert
#include <limits.h> // INT_MAX

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
int timediff_get(struct timespec mark_start) {
	struct timespec mark_end = timediff_mark();
	long diff = ((mark_end.tv_sec - mark_start.tv_sec) * 1000) + ((mark_end.tv_nsec - mark_start.tv_nsec) / 1000000);
	assert(diff >= 0);
	assert(diff <= INT_MAX);

	if (logs_find("timer")) {
		printf("Timer difference: %ld\n", diff);
	}

	return diff & INT_MAX;
}

// Ensures time delta from a previous timestamp is diff_base ms and not more than late_allowed ms late
void timediff_get_verify(struct timespec mark_start, int diff_base, int late_allowed) {
	assert(diff_base >= 0);
	assert(late_allowed >= 0);
	int diff = timediff_get(mark_start);
	if ((diff < diff_base) || (diff > (diff_base + late_allowed))) {
		fprintf(stderr,
			"Expected timer delta between %d and %d, but got %d\n",
			diff_base, diff_base + late_allowed, diff
		);
		abort();
	}
}
