#include <stdio.h> // stderr, perror, fprintf
#include <stdlib.h> // exit, EXIT_FAILURE
#include <errno.h> // errno

#include <winsock2.h> // WSADATA, WSAStartup, WSAGetLastError, WSACleanup
#include <winbase.h> // FormatMessageA, FORMAT_MESSAGE_FROM_SYSTEM, FORMAT_MESSAGE_IGNORE_INSERTS
#include <winnt.h> // MAKELANGID, DWORD, LANG_ENGLISH, SUBLANG_ENGLISH_US

#include "./wsa_shim_perror.h" // wsa_shim_perror

// Automatically initializes WSA by putting startup code in a constructor with global instance

// Links to systems shared libraries
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif // _MSC_VER

// Class for WSA initialization and cleanup
class WSAInit {
	WSADATA wsaData;
public:
	WSAInit() {
		int err = WSAStartup(0x0202, &wsaData);
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

// Replacement for perror function to handle WSA errors
extern "C" void wsa_shim_perror(const char* msg) {
	if (msg == nullptr) return;

	// Outputs normal error if set and clears errno
	if (errno) {
		perror(msg);
		errno = 0;
		return;
	}

	// Gets WSA error string
	char buf[1024];
	DWORD err = WSAGetLastError();
	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		err,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		buf,
		sizeof(buf),
		nullptr
	);

	// Outputs error string
	if (len) {
		fprintf(stderr, "%s: %s\n", msg, buf);
	}
	// Outputs error code if unable to get error string
	else {
		fprintf(stderr, "%s: WSA error code %lu\n", msg, err);
	}
}
