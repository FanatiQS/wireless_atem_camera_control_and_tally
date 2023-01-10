#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stdbool.h> // bool, true, false
#include <time.h> // time, NULL
#include <float.h> // double, DBL_MAX

#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h> // close

#include "../udp.h"
#include "../server.h"
#include "../timer.h"
#include "../relay.h"

// Used to indicate what functions should be accessable when compiled to a library
#define EXPORT __attribute__ ((visibility ("default")))

// Prevents compiler warnnings about unused arguments
#define UNUSED(arg) (void)arg



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
	UNUSED(timer);
	UNUSED(info);
	timerEvent();
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Callback function to process proxy server socket data dispatched from CF
static void proxyCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
	UNUSED(callBackTypes);
	UNUSED(info);
	processProxyData();
	CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

// Callback function to process relay client socket data dispatched from CF
static void relayCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
	UNUSED(callBackTypes);
	UNUSED(info);
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
EXPORT void atem_server_init(CFRunLoopRef rl, const in_addr_t addr) {
	startTime = time(NULL) - CFAbsoluteTimeGetCurrent();

	if (!setupProxy()) {
		perror("Failed to initialize proxy server");
		abort();
	}
	if (!setupRelay()) {
		close(sockProxy);
		perror("Failed to initialize relay client");
		abort();
	}

	if (addr && !relayEnable(addr)) {
		perror("Failed to connect relay client to ATEM address");
		abort();
	}

	addSocketToCFRunLoop(rl, sockProxy, &proxyCallback);
	addSocketToCFRunLoop(rl, sockRelay, &relayCallback);

	timerRef = CFRunLoopTimerCreate(kCFAllocatorDefault, getTimer(), DBL_MAX, 0, 0, &timerCallback, NULL);
	CFRunLoopAddTimer(rl, timerRef, kCFRunLoopDefaultMode);
}

// Enables relay client
EXPORT bool atem_relay_enable(const in_addr_t addr) {
	if (!relayEnable(addr)) {
		return false;
	}
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
	return true;
}

// Disables relay client
EXPORT void atem_relay_disable() {
	relayDisable();
	CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}
