// Include guard
#ifndef POLL_H
#define POLL_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // WSAPoll, LPWSAPOLLFD, SOCKET

// Shims pollfd struct to get POSIX correct signed int for .fd
struct wsa_shim_pollfd {
	union {
		int fd;
		SOCKET win_fd;
	};
	short events;
	short revents;
};
#define pollfd wsa_shim_pollfd

typedef unsigned long nfds_t;

static inline int poll(struct wsa_shim_pollfd* fds, nfds_t nfds, int timeout) {
	errno = 0;
	return WSAPoll((LPWSAPOLLFD)fds, nfds, timeout);
}

#endif // POLL_H
