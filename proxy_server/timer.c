#include <stdint.h> // uint16_t
#include <stdlib.h> // abort
#include <stdio.h> // printf, fprintf, stderr, perror, fflush, stdout
#include <time.h> // timespec

#include "./timer.h"
#include "./udp.h"
#include "./server.h"
#include "../src/atem.h"
#include "./atem_extra.h"
#include "./relay.h"
#include "./debug.h"



// Shim timespec_get for MacOS 10.14 and older
#if defined(__APPLE__) && !defined(TIME_UTC)
#define timespec_get(ts, base) clock_gettime(CLOCK_MONOTONIC, ts)
#define TIME_UTC 0
#warning Shimming timespec_get with clock_gettime
#endif



// Ensures size order of timers are correct
#if ATEM_PING_INTERVAL < ATEM_RESEND_TIME
#error Ping interval has to be larger than resend timer
#endif
#if ATEM_PING_INTERVAL > (ATEM_TIMEOUT * 1000)
#error Ping interval has to be smaller than drop timeout timer
#endif



struct timespec* nextTimer;

static struct timespec* nextResendTimer;
static struct timespec* nextPingTimer;
static struct timespec* nextDropTimer;
static struct timespec pingTimer;
static struct timespec dropTimer;



// Checks if timerA is smaller than timerB
#define TIMER_IS_SMALLER(timerA, timerB)\
	((timerA->tv_sec == timerB->tv_sec) ?\
	(timerA->tv_nsec < timerB->tv_nsec) :\
	(timerA->tv_sec < timerB->tv_sec))

// Sets the next timer pointer to the closest in time argument timer
static void setNextTimerToNearest(struct timespec* timerA, struct timespec* timerB) {
	if (timerA == NULL || (timerB != NULL && TIMER_IS_SMALLER(timerB, timerA))) {
		nextTimer = timerB;
	}
	else {
		nextTimer = timerA;
	}
}



// Sets the value of timer to be n milliseconds in the future
void setTimeout(struct timespec* timer, uint16_t msDelay) {
	if (timespec_get(timer, TIME_UTC) != TIME_UTC) {
		perror("Unable to get time");
		abort();
	}
	timer->tv_nsec += (msDelay - msDelay / 1000 * 1000) * 1000000;
	uint8_t overflow = timer->tv_nsec / 1000000000;
	timer->tv_sec += overflow + msDelay / 1000;
	timer->tv_nsec -= overflow * 1000000000;
}



// Restarts the ping timer, expects it is nextTimer and it just expired
void pingTimerRestart() {
	DEBUG_PRINTF("restarting ping timer\n");

	setTimeout(&pingTimer, ATEM_PING_INTERVAL);

	if (nextResendTimer != NULL) {
		nextTimer = nextResendTimer;
	}
	if (nextDropTimer != NULL && TIMER_IS_SMALLER(nextDropTimer, nextTimer)) {
		nextTimer = nextDropTimer;
	}
}

// Enables and initializes the ping timer
void pingTimerEnable() {
	DEBUG_PRINTF("enabling ping timer\n");

	nextPingTimer = &pingTimer;
	setTimeout(nextPingTimer, ATEM_PING_INTERVAL);

	if (nextTimer == NULL || TIMER_IS_SMALLER(nextPingTimer, nextTimer)) {
		nextTimer = nextPingTimer;
	}
}

// Removes the ping timer when not needed anymore
void pingTimerDisable() {
	DEBUG_PRINTF("disabling ping timer\n");

	if (nextTimer == nextPingTimer) {
		setNextTimerToNearest(nextResendTimer, nextDropTimer);
	}
	nextPingTimer = NULL;
}



// Updates the resend timer pointer when a packet was enqueued to an empty resend queue
void resendTimerAdd(struct timespec* resendTimer) {
	DEBUG_PRINTF("updating next resend timer after addition\n");

	nextResendTimer = resendTimer;
	if (nextTimer == NULL || TIMER_IS_SMALLER(nextResendTimer, nextTimer)) {
		nextTimer = nextResendTimer;
	}
}

// Updates the resend timer pointer when next packet in resend queue was removed
void resendTimerRemove(struct timespec* resendTimer) {
	DEBUG_PRINTF("updating next resend timer after removal\n");

	if (nextTimer == nextResendTimer) {
		setNextTimerToNearest(nextPingTimer, resendTimer);
		if (nextTimer == NULL || (nextDropTimer != NULL && TIMER_IS_SMALLER(nextDropTimer, nextTimer))) {
			nextTimer = nextDropTimer;
		}
	}
	nextResendTimer = resendTimer;
}



// Restarts relay sockets timeout timer, assumes nextTimer is dropTimer
void dropTimerRestart() {
	DEBUG_PRINTF("restarting relay drop timer\n");

	setTimeout(nextDropTimer, ATEM_TIMEOUT * 1000);

	if (nextTimer != nextDropTimer) return;

	if (nextResendTimer == NULL) {
		if (nextPingTimer != NULL) {
			nextTimer = nextPingTimer;
		}
	}
	else if (nextPingTimer == NULL) {
		nextTimer = nextResendTimer;
	}
	else if (TIMER_IS_SMALLER(nextPingTimer, nextResendTimer)) {
		nextTimer = nextPingTimer;
	}
}

// Enables drop timer for when relay client is enabled
void dropTimerEnable() {
	DEBUG_PRINTF("enabling relay drop timer\n");

	nextDropTimer = &dropTimer;
	setTimeout(nextDropTimer, ATEM_TIMEOUT * 1000);
	if (nextTimer == NULL) {
		nextTimer = nextDropTimer;
	}
}

// Disables drop timer for when relay client is disabled
void dropTimerDisable() {
	DEBUG_PRINTF("disabling relay drop timer\n");

	if (nextTimer == nextDropTimer) {
		setNextTimerToNearest(nextResendTimer, nextPingTimer);
	}
	nextDropTimer = NULL;
}



// Calls the next timer event based on the timer that should just have expired
void timerEvent() {
	DEBUG_PRINTF("timer expired\n");

	if (nextTimer == nextPingTimer) {
		pingProxySessions();
	}
	if (nextTimer == nextResendTimer) {
		resendProxyPackets();
	}
	else if (nextTimer == nextDropTimer) {
		reconnectRelaySocket();
	}
	else {
		fprintf(stderr, "Got undefined timer event\n");
		abort();
	}
}

// Gets the duration to the next timer to expire
struct timeval* timeToNextTimerEvent() {
	static struct timeval tv;

	while (nextTimer != NULL) {
		// Gets current time
		struct timespec current;
		timespec_get(&current, TIME_UTC);

		// Gets time difference between nextTimer and current time
		tv.tv_sec = nextTimer->tv_sec - current.tv_sec;
		tv.tv_usec = (int)((nextTimer->tv_nsec - current.tv_nsec) / 1000);
		if (tv.tv_usec < 0) {
			tv.tv_sec--;
			tv.tv_usec += 1000000;
		}

#ifdef DEBUG
		DEBUG_PRINTF(
			"next timer event in %.3f seconds\n",
			(double)tv.tv_sec + (double)tv.tv_usec / 1000000
		);
		printf("\n");
		fflush(stdout);
#endif

		// Returns duration to next timer even only if it should not already have expired
		if (current.tv_sec < nextTimer->tv_sec || (tv.tv_sec == 0 && tv.tv_usec > 0)) {
			return &tv;
		}

		// Catch up to events that should already have occured
		DEBUG_PRINTF("catching up timer event\n");
		timerEvent();
	}

	// Returns NULL if no events are scheduled
	DEBUG_PRINTF("no scheduled timer events\n");
	return NULL;
}
