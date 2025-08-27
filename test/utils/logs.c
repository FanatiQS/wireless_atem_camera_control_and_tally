#include <stdio.h> // FILE, fprintf, stdout, stderr, fflush, printf
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t, NULL
#include <string.h> // strlen, strncmp, strchr, memset
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
void logs_print_buffer(FILE* pipe, void* buf, size_t buf_len) {
	size_t clamp_index;
	size_t len;

	// Gets clamp index if defined
	char* clamp_len = getenv("LOGS_BUFFER_CLAMP");
	if (clamp_len) {
		clamp_index = (size_t)atoi(clamp_len) * PRINT_BYTE_LEN;
		if (clamp_index == 0) {
			fprintf(stderr, "Invalid LOGS_BUFFER_CLAMP value: %s\n", clamp_len);
			abort();
		}
		len = (buf_len < clamp_index) ? buf_len : clamp_index;
	}
	else {
		clamp_index = buf_len;
		len = buf_len;
	}

	// Prints buffer to clamp index or end of buffer
	fprintf(pipe, "\t");
	for (size_t i = 0; i < len; i++) {
		if (i != 0 && !(i % PRINT_BYTE_LEN)) {
			fprintf(pipe, "\n\t");
		}
		fprintf(pipe, "%02x ", ((uint8_t*)buf)[i]);
	}

	if (buf_len > clamp_index) {
		fprintf(pipe, "\n\t... (%zu)\n", buf_len - len);
	}
	else {
		fprintf(pipe, "\n");
	}
}

// Clearly prints multiline string
void logs_print_string(FILE* pipe, const char* str, bool chunked) {
	assert(str != NULL);

	if (str[0] == '\0') {
		return;
	}

	fprintf(pipe, "\x1b[7m");
	do {
		const char c = *str;
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
	fprintf(pipe, "\x1b[27m");

	if (!chunked) {
		fprintf(pipe, "\n");
	}
}

// Prints progress bar that is only visible until flush of standard out
void logs_print_progress(size_t index, size_t len) {
	assert(index < len);

	// Prints progress bar
	char bar[64];
	const size_t progress = index * sizeof(bar) / len;
	memset(bar, '#', progress);
	memset(bar + progress, '.', sizeof(bar) - progress);
	printf("[%.*s] %zu/%zu", (int)sizeof(bar), bar, index, len);
	fflush(stdout);

	// Removes progress bar on next flush to console
	printf("\r\x1b[K");
}
