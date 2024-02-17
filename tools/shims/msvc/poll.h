// Include guard
#ifndef POLL_H
#define POLL_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // WSAPoll

#define poll(fds, nfds, timeout) WSAPoll(fds, nfds, timeout)

#endif // POLL_H
