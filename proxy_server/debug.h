// Include guard
#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h> // printf

#define DEBUG_PRINT_LINE_LIMIT 12

#ifdef DEBUG
#define DEBUG_PRINTF(...) printf("DEBUG " __VA_ARGS__)
#define DEBUG_PRINT_BUFFER(buf, len, ...)\
	do {\
		DEBUG_PRINTF(__VA_ARGS__);\
		for (int i = 0; i < len; i++) {\
			if ((i % DEBUG_PRINT_LINE_LIMIT) == 0) {\
				printf("\n\t");\
			}\
			printf("%02x ", buf[i]);\
		}\
		printf("\n");\
	} while (0)
#else
#define DEBUG_PRINTF(...)
#define DEBUG_PRINT_BUFFER(buf, len, ...)
#endif

// Ends include guard
#endif
