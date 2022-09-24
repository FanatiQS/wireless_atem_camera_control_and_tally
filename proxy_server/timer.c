#include <stdint.h> // uint16_t
#include <stdlib.h> // abort
#include <stdio.h> // perror

#include "./timer.h"
#include "./server.h"
#include "../src/atem.h"
#include "./atem_extra.h"
#include "./relay.h"
#include "./debug.h"



// Ensures size order of timers are correct
#if ATEM_PING_INTERVAL < ATEM_RESEND_TIME
#error Ping interval has to be larger than resend timer
#endif
#if ATEM_PING_INTERVAL > (ATEM_TIMEOUT * 1000)
#error Ping interval has to be smaller than drop timeout timer
#endif



static struct timeval* nextTimer;

static struct timeval* nextResendTimer;
static struct timeval* nextPingTimer;
static struct timeval* nextDropTimer;
static struct timeval pingTimer;
static struct timeval dropTimer;



// Sets the next timer pointer to the closest in time argument timer
static void setNextTimerToNearest(struct timeval* timerA, struct timeval* timerB) {
	if (timerA == NULL || (timerB != NULL && timercmp(timerB, timerA, <))) {
		nextTimer = timerB;
	}
	else {
		nextTimer = timerA;
	}
}



// Sets the value of timer to be n milliseconds in the future
void setTimeout(struct timeval* timer, uint16_t msDelay) {
	if (gettimeofday(timer, NULL) == -1) {
		perror("Unable to get time of day");
		abort();
	}
	timer->tv_usec += msDelay * 1000;
	uint16_t overflow = timer->tv_usec / 1000000;
	timer->tv_usec -= overflow * 1000000;
	timer->tv_sec += overflow;
}



// Restarts the ping timer when, assimes it just expired
void pingTimerRestart() {
	setTimeout(&pingTimer, ATEM_PING_INTERVAL);

	if (nextResendTimer != NULL) {
		nextTimer = nextResendTimer;
	}
	if (nextDropTimer != NULL && timercmp(nextDropTimer, nextTimer, <)) {
		nextTimer = nextDropTimer;
	}

	DEBUG_PRINT("reset ping timer\n");
}

// Enables and initializes the ping timer
void pingTimerEnable() {
	DEBUG_PRINT("enabled ping timer\n");

	nextPingTimer = &pingTimer;
	setTimeout(nextPingTimer, ATEM_PING_INTERVAL);

	if (nextTimer == NULL || timercmp(nextPingTimer, nextTimer, <)) {
		nextTimer = nextPingTimer;
	}
}

// Removes the ping timer when not needed anymore
void pingTimerDisable() {
	DEBUG_PRINT("disabled ping timer\n");

	if (nextTimer == nextPingTimer) {
		setNextTimerToNearest(nextResendTimer, nextDropTimer);
	}
	nextPingTimer = NULL;
}



// Updates the resend timer pointer when a packet was enqueued to an empty resend queue
void resendTimerAdd(struct timeval* resendTimer) {
	DEBUG_PRINT("updated next resend timer after add\n");

	nextResendTimer = resendTimer;
	if (nextTimer == NULL || timercmp(nextResendTimer, nextTimer, <)) {
		nextTimer = nextResendTimer;
	}
}

// Updates the resend timer pointer when next packet in resend queue was removed
void resendTimerRemove(struct timeval* resendTimer) {
	DEBUG_PRINT("updated next resend timer after removal\n");

	if (nextTimer == nextResendTimer) {
		setNextTimerToNearest(nextPingTimer, resendTimer);
		if (nextTimer == NULL || (nextDropTimer != NULL && timercmp(nextDropTimer, nextTimer, <))) {
			nextTimer = nextDropTimer;
		}
	}
	nextResendTimer = resendTimer;
}



// Restarts relay sockets timeout timer
void dropTimerRestart() {
	DEBUG_PRINT("reset relay drop timer\n");

	setTimeout(nextDropTimer, ATEM_TIMEOUT * 1000);
	if (nextTimer == nextDropTimer) {
		if (nextResendTimer == NULL) {
			if (nextPingTimer != NULL) {
				nextTimer = nextPingTimer;
			}
		}
		else if (nextPingTimer == NULL) {
			nextTimer = nextResendTimer;
		}
		else if (timercmp(nextPingTimer, nextResendTimer, <)) {
			nextTimer = nextPingTimer;
		}
	}
}

// Enables drop timer when relay client was initialized
void dropTimerEnable() {
	DEBUG_PRINT("enabled relay drop timer\n");

	nextDropTimer = &dropTimer;
	setTimeout(nextDropTimer, ATEM_TIMEOUT * 1000);
	if (nextTimer == NULL) {
		nextTimer = nextDropTimer;
	}
}



// Calls the next timer event based on the timer that should just have expired
void timerEvent() {
	DEBUG_PRINT("timer expired\n");

	if (nextTimer == nextResendTimer) {
		resendProxyPackets();
	}
	else if (nextTimer == nextDropTimer) {
		reconnectRelaySocket();
	}
	else if (nextTimer == nextPingTimer) {
		pingProxySessions();
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
		struct timeval current;
		gettimeofday(&current, NULL);
		timersub(nextTimer, &current, &tv);

#ifdef DEBUG
		printf("\n");
#endif
		DEBUG_PRINT(
			"next timer event in %.4f seconds\n",
			(float)tv.tv_sec + (float)tv.tv_usec / 1000000
		);

		// Returns duration to next timer even only if it should not already have expired
		if (current.tv_sec < nextTimer->tv_sec || (tv.tv_sec == 0 && tv.tv_usec > 0)) {
			return &tv;
		}

		// Catch up to events that should already have occured
		DEBUG_PRINT("catch up timer event call\n");
		timerEvent();
	}

	// Returns NULL if no events are scheduled
	DEBUG_PRINT("no scheduled timer events\n");
	return NULL;
}
