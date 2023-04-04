#ifndef TIMER_H
#define TIMER_H

#include <stdint.h> // uint32_t
#include <time.h> // struct timespec

struct timespec timer_current_get();
struct timespec timer_start();
uint32_t timer_end(struct timespec);

#endif // TIMER_H
