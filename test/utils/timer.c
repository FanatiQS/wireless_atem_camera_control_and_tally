#include <stdint.h> // uint32_t
#include <time.h> // struct timespec, timespec_get
#include <stdio.h> // perror

#include "./runner.h" // testrunner_abort



// Shim timespec_get for MacOS 10.14 and older
#ifndef TIME_UTC
#define TIME_UTC 0
#define timespec_get(ts, base) clock_gettime(CLOCK_REALTIME, ts)
#endif



// Gets current time
struct timespec timer_current_get() {
	struct timespec timerStart;
	if (timespec_get(&timerStart, TIME_UTC) != TIME_UTC) {
		perror("Failed to start timer");
		testrunner_abort();
	}
	return timerStart;
}

// Gets start time for timer
struct timespec timer_start() {
	return timer_current_get();
}

// Gets time diff from start time in milliseconds
uint32_t timer_end(struct timespec timerStart) {
	struct timespec timerEnd = timer_current_get();
	uint32_t timeDiff = (timerEnd.tv_sec - timerStart.tv_sec) * 1000;
	timeDiff += (timerEnd.tv_nsec - timerStart.tv_nsec) / 1000000;
	return timeDiff;
}
