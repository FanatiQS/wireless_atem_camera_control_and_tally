#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stdbool.h> // false
#include <time.h> // time, NULL
#include <float.h> // double, DBL_MAX

#include <CoreFoundation/CoreFoundation.h>

#include "../udp.h"
#include "../server.h"
#include "../timer.h"
#include "../relay.h"



// Timer variables used to dispatch proxy server timers in CF
static CFRunLoopTimerRef timerRef;
static CFAbsoluteTime startTime;

// Gets timestamp for next timer event to be used with CF timers
static double getTimer() {
	timeToNextTimerEvent();
	if (nextTimer == NULL) return DBL_MAX;
	return (double)nextTimer->tv_sec - startTime + (double)nextTimer->tv_nsec / 1000000000;
}

// Callback function to process timer event from CF
static void timerCallback(CFRunLoopTimerRef timer, void* info) {
	timerEvent();
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Callback function to process proxy server socket data dispatched from CF
static void proxyCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
	processProxyData();
	CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Callback function to process relay client socket data dispatched from CF
static void relayCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
	processProxyData();
	CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Attaches a callback function to a socket in a CF run loop
static void addSocketToCFRunLoop(CFRunLoopRef rl, int sock, CFFileDescriptorCallBack callback) {
	const CFFileDescriptorRef fdRef = CFFileDescriptorCreate(kCFAllocatorDefault, sock, false, callback, NULL);
	if (fdRef == NULL) {
		fprintf(stderr, "Unable to create CF file descriptor\n");
		abort();
	}
	CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
	const CFRunLoopSourceRef source = CFFileDescriptorCreateRunLoopSource(kCFAllocatorDefault, fdRef, 0);
	if (source == NULL) {
		fprintf(stderr, "Unable to create CF run loop source\n");
		abort();
	}
	CFRunLoopAddSource(rl, source, kCFRunLoopDefaultMode);
	CFRelease(source);
}

// Initializes proxy server
void atem_server_init(CFRunLoopRef rl, const in_addr_t addr) {
	startTime = time(NULL) - CFAbsoluteTimeGetCurrent();

	setupProxy();
	setupRelay();

	addSocketToCFRunLoop(rl, sockProxy, &proxyCallback);
	addSocketToCFRunLoop(rl, sockRelay, &relayCallback);

	if (addr) {
		relayEnable(addr);
	}

	timerRef = CFRunLoopTimerCreate(kCFAllocatorDefault, getTimer(), DBL_MAX, 0, 0, &timerCallback, NULL);
	CFRunLoopAddTimer(rl, timerRef, kCFRunLoopDefaultMode);
}

// Enables relay client
void atem_relay_enable(const in_addr_t addr) {
	relayEnable(addr);
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Disables relay client
void atem_relay_disable() {
	relayDisable();
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}
