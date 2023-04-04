#include <stdio.h> // printf
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <string.h> // strlen
#include <stdbool.h> // bool

#include "./logs.h"

#define PRINT_BUF_LINE_LEN 78
#define TAB_WIDTH 8



// Flags indicating how printing should behave
bool print_disabled = false;
bool print_buffer_disabled = true;

// Prints a buffer to stdout as hex bytes
void print_buffer(char* label, uint8_t* buf, size_t len) {
	// Only prints buffer info if printing is enabled
	if (print_buffer_disabled) return;
	if (print_disabled) return;

	// Prints label for buffer
	printf("%s: ", label);

	// Only prints on multiple lines if buffer does not fit on same line as label
	bool multiline = strlen(label) + strlen(": ") + len * 3 > PRINT_BUF_LINE_LEN;

	for (int i = 0; i < len; i++) {
		// Wraps line to next line if index is wrapped
		if (multiline && !(i % ((PRINT_BUF_LINE_LEN - TAB_WIDTH) / 3))) {
			printf("\n\t");
		}

		// Prints byte in buffer as hex
		printf("%02x ", buf[i]);
	}

	// Always ends buffer print on a clean line
	printf("\n");
}
