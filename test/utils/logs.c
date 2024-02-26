#include <stdio.h> // FILE, fprintf, stderr, printf
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t, NULL
#include <string.h> // strlen, strncmp, strchr
#include <stdbool.h> // bool, true, false
#include <stdlib.h> // getenv, atoi, abort
#include <assert.h> // assert

#include "./logs.h"

#define PRINT_BYTE_LEN (32)

// Searches for match in LOGS environment variable to detect if certain type of printing should be done
bool logs_find(const char* match) {
	// Gets environment variable for array of logs enabled
    const char* start = getenv("LOGS");
	if (start == NULL) {
		return false;
	}

	// Searches through string array for match
    const char* end;
    while ((end = strchr(start, ','))) {
        if (!strncmp(start, match, (size_t)(end - start))) {
            return true;
        }
        start = end + 1;
    }
    if (!strncmp(start, match, strlen(start))) {
        return true;
    }

    return false;
}

// Prints binary buffer in hex
void logs_print_buffer(FILE* pipe, uint8_t* buf, size_t bufLen) {
	size_t clampIndex;
	size_t len;

	// Gets clamp index if defined
	char* clampLen = getenv("LOGS_BUFFER_CLAMP");
	if (clampLen) {
		clampIndex = (size_t)atoi(clampLen) * PRINT_BYTE_LEN;
		if (clampIndex == 0) {
			fprintf(stderr, "Invalid LOGS_BUFFER_CLAMP value: %s\n", clampLen);
			abort();
		}
		len = (bufLen < clampIndex) ? bufLen : clampIndex;
	}
	else {
		clampIndex = bufLen;
		len = bufLen;
	}

	// Prints buffer to clamp index or end of buffer
	fprintf(pipe, "\t");
	for (size_t i = 0; i < len; i++) {
		if (i != 0 && !(i % PRINT_BYTE_LEN)) {
			fprintf(pipe, "\n\t");
		}
		fprintf(pipe, "%02x ", buf[i]);
	}

	if (bufLen > clampIndex) {
		printf("\n\t... (%zu)\n", bufLen - len);
	}
	else {
		fprintf(pipe, "\n");
	}
}

// Clearly prints multiline string
void logs_print_string(FILE* pipe, const char* str) {
	assert(pipe == stdout || pipe == stderr);
	assert(str != NULL);

	if (*str == '\0') {
		return;
	}

	fprintf(pipe, "\x1b[7m");
	do {
		const unsigned char c = *str;
		if (c == '\r') {
			fprintf(pipe, "\\r");
		}
		else if (c == '\n') {
			fprintf(pipe, "\\n\n");
		}
		else {
			assert(c >= ' ' || c == '\t');
			fprintf(pipe, "%c", c);
		}
	} while (*(++str) != '\0');
	fprintf(pipe, "\x1b[m\n");
}
