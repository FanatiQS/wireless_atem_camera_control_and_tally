#include <stdio.h> // stderr, perror, fwprintf
#include <stdlib.h> // exit, EXIT_FAILURE
#include <errno.h> // errno
#include <wchar.h> // wchar_t

#include <winsock2.h> // WSADATA, WSAStartup, WSAGetLastError, WSACleanup
#include <winbase.h> // FormatMessage, FORMAT_MESSAGE_FROM_SYSTEM, FORMAT_MESSAGE_IGNORE_INSERTS
#include <winnt.h> // MAKELANGID, DWORD, LANG_NEUTRAL, SUBLANG_DEFAULT

// Automatically initializes WSA by putting startup code in a constructor with global instance

// Links to systems shared libraries
#pragma comment(lib, "ws2_32.lib")

// Class for WSA initialization and cleanup
class WSAInit {
	WSADATA wsaData;
public:
	WSAInit() {
		int err = WSAStartup(0x0202, &wsaData);
		if (!err) return;
		fwprintf(stderr, L"Failed to initialize WSA: %d\n", err);
		exit(EXIT_FAILURE);
	}
	~WSAInit() {
		WSACleanup();
	}
};

// Global WSA class instance to force initialization to be called before main
static WSAInit wsa;

// Replacement for perror function to handle WSA errors
extern "C" void WSAperror(const char* msg) {
	if (msg == nullptr) return;

	// Outputs normal error if set and clears errno
	if (errno) {
		perror(msg);
		errno = 0;
		return;
	}

	// Gets WSA error string
	wchar_t buf[1024];
	DWORD err = WSAGetLastError();
	DWORD len = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		buf,
		sizeof(buf) / sizeof(buf[0]),
		nullptr
	);

	// Outputs error string
	if (len) {
		fwprintf(stderr, L"%S: %s\n", msg, buf);
	}
	// Outputs error code if unable to get error string
	else {
		fwprintf(stderr, L"%S: WSA error code %d\n", msg, err);
	}
}
