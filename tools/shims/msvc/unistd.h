// Include guard
#ifndef UNISTD_H
#define UNISTD_H

#include <winsock2.h> // closesocket
#include <windows.h> // Sleep

#define sleep(delay) Sleep(delay / 1000)
#define usleep(delay) Sleep(delay * 1000)

static inline int wsa_shim_close(int fd) {
	errno = 0;
	return closesocket((SOCKET)fd);
}
#define close wsa_shim_close

#endif // UNISTD_H
