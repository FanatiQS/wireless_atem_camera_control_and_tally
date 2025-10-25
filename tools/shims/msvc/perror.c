#include <stddef.h> // NULL
#include <errno.h> // errno
#include <stdio.h> // perror, fprintf, stderr

#define WIN32_LEAN_AND_MEAN
#include <winbase.h> // FormatMessageA, FORMAT_MESSAGE_FROM_SYSTEM, FORMAT_MESSAGE_IGNORE_INSERTS
#include <winnt.h> // MAKELANGID, DWORD, LANG_ENGLISH, SUBLANG_ENGLISH_US

#include "./sys/socket.h" // wsa_shim_perror

// Replacement for perror function to also handle WSA errors stored as negative errno values
void wsa_shim_perror(const char* msg) {
	// Outputs normal errno error message
	if (errno >= 0) {
		perror(msg);
		return;
	}

	// Gets WSA error string
	char buf[1024];
	int err = -errno;
	DWORD len = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		err,
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
			fprintf(stderr, "WSA error code %d\n", err);
		}
		else {
			fprintf(stderr, "%s: WSA error code %d\n", msg, err);
		}
	}
}
