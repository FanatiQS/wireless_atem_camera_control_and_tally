// Include guard
#ifndef SHIM_TIME_H
#define SHIM_TIME_H

// Prevents warnings about include_next being a language extension
#pragma GCC system_header

// Includes standard header file
#include_next <time.h> // struct timespec, clock_gettime, CLOCK_REALTIME

// Shim timespec_get for non-C11 platforms with POSIX.1-2001 support like MacOS 10.14 and earlier
#define TIME_UTC 0
#define timespec_get(ts, base) clock_gettime(CLOCK_REALTIME, ts)

#endif // SHIM_TIME_H
