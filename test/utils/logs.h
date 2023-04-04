#ifndef LOGS_H
#define LOGS_H

#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <stdbool.h> // bool
#include <stdio.h> // printf, fprintf, stderr

extern bool print_disabled;
extern bool print_buffer_disabled;

void print_buffer(char* label, uint8_t* buf, size_t len);

#define print_info(...) do { if (print_disabled) break; printf(__VA_ARGS__); } while (0)
#define print_debug(...) fprintf(stderr, __VA_ARGS__)

#endif // LOGS_H
