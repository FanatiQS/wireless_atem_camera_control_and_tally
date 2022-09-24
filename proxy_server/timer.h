// Include guard
#ifndef TIMER_H
#define TIMER_H

#include <time.h> // timespec

#include "./udp.h" // timeval

void setTimeout(struct timespec* timer, uint16_t msDelay);

void pingTimerRestart();
void pingTimerEnable();
void pingTimerDisable();

void resendTimerAdd(struct timespec* resendTimer);
void resendTimerRemove(struct timespec* resendTimer);

void dropTimerEnable();
void dropTimerRestart();

struct timeval* timeToNextTimerEvent();
void timerEvent();

// Ends include guard
#endif
