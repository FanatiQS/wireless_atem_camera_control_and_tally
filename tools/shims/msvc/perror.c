#include <stddef.h> // NULL
#include <errno.h> // errno
#include <stdio.h> // perror, fprintf, stderr

#include <winsock2.h> // WSAGetLastError
#include <winbase.h> // FormatMessageA, FORMAT_MESSAGE_FROM_SYSTEM, FORMAT_MESSAGE_IGNORE_INSERTS
#include <winnt.h> // MAKELANGID, DWORD, LANG_ENGLISH, SUBLANG_ENGLISH_US

#include "./sys/socket.h" // wsa_shim_perror

// Replacement for perror function to also handle WSA errors stored as negative errno values
void wsa_shim_perror(const char* msg) {
	// Outputs normal error if set and clears errno
	if (errno >= 0) {
		perror(msg);
		return;
	}

	// Gets WSA error string
	char buf[1024];
	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		-errno,
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		buf,
		sizeof(buf),
		NULL
	);

	// Outputs error string
	if (len) {
		if (msg == NULL || msg[0] == '\0') {
			fprintf(stderr, "%s\n", buf);
		}
		else {
			fprintf(stderr, "%s: %s\n", msg, buf);
		}
	}
	// Outputs error code if unable to get error string
	else {
		if (msg == NULL || msg[0] == '\0') {
			fprintf(stderr, "WSA error code %lu\n", err);
		}
		else {
			fprintf(stderr, "%s: WSA error code %lu\n", msg, err);
		}
	}
}
