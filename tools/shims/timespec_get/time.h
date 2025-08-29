// Include guard
#ifndef SHIM_TIME_H
#define SHIM_TIME_H

// Includes standard header file
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-include-next"
#include_next <time.h>
#pragma GCC diagnostic pop

/*
 * Shims timespec_get for non-C11 platforms that has POSIX.1-2001 support.
 * This includes MacOS before 10.15 and Linux with glibc older than 2.16.
 */
#define TIME_UTC 0
#define timespec_get(ts, base) clock_gettime(CLOCK_REALTIME, ts)

#endif // SHIM_TIME_H
