// Include guard
#ifndef TIMER_H
#define TIMER_H

#include <time.h> // timespec

#include "./udp.h" // timeval

void setTimeout(struct timespec* timer, uint16_t msDelay);

void pingTimerRestart(void);
void pingTimerEnable(void);
void pingTimerDisable(void);

void resendTimerAdd(struct timespec* resendTimer);
void resendTimerRemove(struct timespec* resendTimer);

void dropTimerEnable(void);
void dropTimerDisable(void);
void dropTimerRestart(void);

struct timeval* timeToNextTimerEvent(void);
void timerEvent(void);

// Ends include guard
#endif
