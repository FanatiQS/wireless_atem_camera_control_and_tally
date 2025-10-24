#include <stdio.h> // stderr, fprintf
#include <stdlib.h> // exit, EXIT_FAILURE

#include <winsock2.h> // WSADATA, WSAStartup, WSACleanup

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
