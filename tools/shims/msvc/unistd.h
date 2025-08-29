// Include guard
#ifndef UNISTD_H
#define UNISTD_H

#include <winsock2.h> // closesocket

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-include-next"
#include_next <unistd.h> // sleep, usleep
#pragma GCC diagnostic pop
#else
#include <windows.h> // Sleep

static inline unsigned sleep(unsigned seconds) {
	Sleep(seconds * 1000);
	return 0;
}

static inline int usleep(unsigned useconds) {
	Sleep(useconds / 1000);
	return 0;
}
#endif // _MSC_VER



static inline int wsa_shim_close(int fd) {
	errno = 0;
	return closesocket((SOCKET)fd);
}
#define close wsa_shim_close

#endif // UNISTD_H
