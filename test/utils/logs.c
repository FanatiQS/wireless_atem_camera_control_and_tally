#include <stdio.h> // FILE, fprintf
#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <string.h> // strlen
#include <stdbool.h> // bool

#include "./logs.h"

#define PRINT_BYTE_LEN (32)

// Switches indicating if network send/recv functions should print data
bool logs_enable_send = false;
bool logs_enable_recv = false;

// Prints binary buffer in hex
void logs_print_buffer(FILE* pipe, uint8_t* buf, size_t bufLen) {
	printf("\t");
	for (size_t i = 0; i < bufLen; i++) {
		if (i != 0 && !(i % PRINT_BYTE_LEN)) fprintf(pipe, "\n\t");
		fprintf(pipe, "%02x ", buf[i]);
	}
	fprintf(pipe, "\n");
}

// Prints multiline string with clear start and end markers
void logs_print_string(FILE* pipe, char* str) {
	fprintf(pipe, "=====START=====\n%s\n=====END=====\n", str);
}
