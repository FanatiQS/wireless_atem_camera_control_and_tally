// Include guard
#ifndef UNISTD_H
#define UNISTD_H

#include <winsock2.h> // closesocket
#include <windows.h> // Sleep

#define close(fd) closesocket(fd)

#define sleep(delay) Sleep(delay / 1000)

#endif // UNISTD_H
