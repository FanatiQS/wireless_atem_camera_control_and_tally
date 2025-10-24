// Include guard
#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

#include <stddef.h> // size_t
#include <limits.h> // INT_MAX
#include <errno.h> // errno, EMFILE
#include <assert.h> // _Static_assert

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h> // socket, SOCKET, INVALID_SOCKET, SOCKET_ERROR, bind, recvfrom, sendto, AF_INET, SOCK_DGRAM, struct sockaddr, connect, recv, send, setsockopt, shutdown, SD_RECEIVE, SD_SEND, SD_BOTH
#include <ws2tcpip.h> // socklen_t

#include "../wsa_shim_perror.h" // wsa_shim_perror

#ifdef _MSC_VER
typedef long long ssize_t;
#endif // _MSC_VER

#define SHUT_RD   SD_RECEIVE
#define SHUT_WR   SD_SEND
#define SHUT_RDWR SD_BOTH

#define perror(msg) wsa_shim_perror(msg)

// Asserts windows defined types are the same or at least big enough
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpre-c11-compat"
#endif // __clang__
static_assert(SOCKET_ERROR == -1, "SOCKET_ERROR is -1 on POSIX");
static_assert((int)INVALID_SOCKET == -1, "INVALID_SOCKET is -1 on POSIX");
static_assert(sizeof(SOCKET) >= sizeof(int), "winsock2 SOCKET type is an int on POSIX");
static_assert(sizeof(socklen_t) == sizeof(int), "socklen_t in POSIX uses int in winsock2");
static_assert(sizeof(ssize_t) >= sizeof(int), "ssize_t needs to hold at least an int");
#ifdef __clang__
#pragma clang diagnostic pop
#endif // __clang__



static inline int wsa_shim_accept(int sock, struct sockaddr* addr, socklen_t* len) {
	SOCKET sock_accepted = accept((SOCKET)sock, addr, len);
	if (sock_accepted > INT_MAX) {
		if (sock_accepted != INVALID_SOCKET) {
			closesocket(sock_accepted);
			errno = EMFILE;
			return -1;
		}
		errno = -WSAGetLastError();
	}
	return (int)sock_accepted;
}

static inline int wsa_shim_bind(int sock, const struct sockaddr* addr, socklen_t len) {
	int result = bind((SOCKET)sock, addr, len);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline int wsa_shim_connect(int sock, const struct sockaddr* addr, socklen_t len) {
	int result = connect((SOCKET)sock, addr, len);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline int wsa_shim_listen(int sock, int backlog) {
	int result = listen((SOCKET)sock, backlog);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline ssize_t wsa_shim_recv(int sock, void* buf, size_t len, int flags) {
	int result = recv((SOCKET)sock, buf, (len > INT_MAX) ? -1 : (int)len, flags);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline ssize_t wsa_shim_recvfrom(
	int sock,
	void* restrict buf,
	size_t len,
	int flags,
	struct sockaddr* restrict addr,
    socklen_t* restrict addr_len
) {
	int result = recvfrom((SOCKET)sock, buf, (len > INT_MAX) ? -1 : (int)len, flags, addr, addr_len);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline ssize_t wsa_shim_send(int sock, const void* buf, size_t len, int flags) {
	int result = send((SOCKET)sock, buf, (len > INT_MAX) ? -1 : (int)len, flags);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline ssize_t wsa_shim_sendto(
	int sock,
	const void* buf,
	size_t len,
    int flags,
	const struct sockaddr* dest_addr,
    socklen_t dest_len
) {
	int result = sendto((SOCKET)sock, buf, (len > INT_MAX) ? -1 : (int)len, flags, dest_addr, dest_len);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline int wsa_shim_setsockopt(int sock, int lvl, int opt, void* value, socklen_t len) {
	// Replaces POSIX timeval for SO_RCVTIMEO/SO_SNDTIMEO with windows DWORD non-standard implementation
	DWORD timeout;
	if (lvl == SOL_SOCKET && (opt == SO_RCVTIMEO || opt == SO_SNDTIMEO)) {
		struct timeval* tv = value;
		timeout = tv->tv_sec * 1000 + tv->tv_usec / 1000;
		value = &timeout;
	}

	int result = setsockopt((SOCKET)sock, lvl, opt, value, len);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline int wsa_shim_shutdown(int sock, int how) {
	int result = shutdown((SOCKET)sock, how);
	if (result == SOCKET_ERROR) {
		errno = -WSAGetLastError();
	}
	else {
		assert(result == 0);
	}
	return result;
}

static inline int wsa_shim_socket(int domain, int type, int protocol) {
	SOCKET sock = socket(domain, type, protocol);
	if (sock > INT_MAX) {
		if (sock != INVALID_SOCKET) {
			closesocket(sock);
			errno = EMFILE;
			return -1;
		}
		errno = -WSAGetLastError();
	}
	return (int)sock;
}

#define accept          wsa_shim_accept
#define bind            wsa_shim_bind
#define connect         wsa_shim_connect
#define getpeername     wsa_shim_getpeername
#define getsockname     wsa_shim_getsockname
#define getsockopt      wsa_shim_getsockopt
#define listen          wsa_shim_listen
#define recv            wsa_shim_recv
#define recvfrom        wsa_shim_recvfrom
#define recvmsg         wsa_shim_recvmsg
#define send            wsa_shim_send
#define sendmsg         wsa_shim_sendmsg
#define sendto          wsa_shim_sendto
#define setsockopt      wsa_shim_setsockopt
#define shutdown        wsa_shim_shutdown
#define socket          wsa_shim_socket

#endif // SYS_SOCKET_H
