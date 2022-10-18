#include <stdio.h> // fprintf, stderr
#include <stdlib.h> // abort
#include <stdbool.h> // false
#include <time.h> // time, NULL
#include <float.h> // double, DBL_MAX

#include <CoreFoundation/CoreFoundation.h>

#include "./udp.h"
#include "./server.h"
#include "./timer.h"
#include "./relay.h"



static CFRunLoopTimerRef timerRef;
CFAbsoluteTime startTime;

static double getTimer() {
    timeToNextTimerEvent();
    if (nextTimer == NULL) return DBL_MAX;
    return (double)nextTimer->tv_sec - startTime + (double)nextTimer->tv_nsec / 1000000000;
}

static void timerCallback(CFRunLoopTimerRef timer, void* info) {
    timerEvent();
    CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

static void proxyCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
    processProxyData();
    CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
    CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

static void relayCallback(CFFileDescriptorRef fdRef, CFOptionFlags callBackTypes, void* info) {
    processProxyData();
    CFFileDescriptorEnableCallBacks(fdRef, kCFFileDescriptorReadCallBack);
    CFRunLoopTimerSetNextFireDate(timerRef, getTimer());
}

static void addSocketToCFRunLoop(int sock, void(*callback)(CFFileDescriptorRef, CFOptionFlags, void*)) {
    CFFileDescriptorRef fdRef = CFFileDescriptorCreate(kCFAllocatorDefault, sock, false, callback, NULL);
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
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopDefaultMode);
    CFRelease(source);
}

void atemServerInit(const in_addr_t addr) {
    startTime = time(NULL) - CFAbsoluteTimeGetCurrent();

    setupProxy();
    setupRelay();

    addSocketToCFRunLoop(sockProxy, &proxyCallback);
    addSocketToCFRunLoop(sockRelay, &relayCallback);

    relayEnable(addr);

    timerRef = CFRunLoopTimerCreate(kCFAllocatorDefault, getTimer(), DBL_MAX, 0, 0, &timerCallback, NULL);
    CFRunLoopAddTimer(CFRunLoopGetMain(), timerRef, kCFRunLoopDefaultMode);
}
