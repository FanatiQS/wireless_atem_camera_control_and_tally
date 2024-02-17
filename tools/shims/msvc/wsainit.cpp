#include <stdio.h> // fprintf, stderr, fputs
#include <string.h> // strerror_s
#include <stdlib.h> // exit, EXIT_FAILURE
#include <errno.h> // errno
#include <stddef.h> // NULL
#include <wchar.h> // wchar_t

#include <winsock2.h> // WSADATA, WSAStartup, WSAGetLastError, WSACleanup
#include <windef.h> // MAKEWORD
#include <winbase.h> // FormatMessage, FORMAT_MESSAGE_FROM_SYSTEM, FORMAT_MESSAGE_IGNORE_INSERTS
#include <winnt.h> // MAKELANGID, DWORD

// Automatically initializes WSA by putting startup code in a constructor with global instance

// Links to systems shared libraries
#pragma comment(lib, "ws2_32.lib")

// Class for WSA initialization and cleanup
class WSAInit {
	WSADATA wsaData;
public:
	WSAInit() {
		int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (!err) return;
		fprintf(stderr, "Failed to initialize WSA: %d\n", err);
		exit(EXIT_FAILURE);
	}
	~WSAInit() {
		WSACleanup();
	}
};

// Global WSA class instance to force initialization to be called before main
static WSAInit wsa;

// Overwrites perror function to handle WSA errors
extern "C" void _perror(const char* msg) {
	char buf[1024];
	if (!msg) return;

	// Outputs error message from argument
	fputs(msg, stderr);

	// Outputs normal error if set
	if (errno) {
		strerror_s(buf, sizeof(buf), errno);
		fprintf(stderr, ": %s\n", buf);
		errno = 0;
		return;
	}

	// Gets WSA error string
	DWORD err = WSAGetLastError();
	DWORD len = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(wchar_t*)buf,
		sizeof(buf) / sizeof(wchar_t),
		NULL
	);

	// Outputs error string
	if (len) {
		fprintf(stderr, ": %ls\n", (wchar_t*)buf);
	}
	// Outputs error code if unable to get error string
	else {
		fprintf(stderr, ": WSA error code %d\n", err);
	}
}
