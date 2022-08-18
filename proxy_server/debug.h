// Include guard
#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_PRINT_LINE_LIMIT 12

#ifdef DEBUG
#define DEBUG_PRINT(...) printf("DEBUG " __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

// Ends include guard
#endif
