// Include guard
#ifndef TIMER_H
#define TIMER_H

#include "./timer_unix.h"

extern struct timeval* nextTimer;

void setTimeout(struct timeval* timer, uint16_t msDelay);

void pingTimerRestart();
void pingTimerEnable();
void pingTimerDisable();

void resendTimerAdd(struct timeval* resendTimer);
void resendTimerRemove();

void dropTimerEnable();
void dropTimerRestart();

struct timeval* timeToNextTimerEvent();
void timerEvent();

// Ends include guard
#endif
