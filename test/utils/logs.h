// Include guard
#ifndef LOGS_H
#define LOGS_H

#include <stdint.h> // uint8_t
#include <stddef.h> // size_t
#include <stdio.h> // FILE
#include <stdbool.h> // bool

bool logs_find(const char* match);
void logs_print_buffer(FILE* pipe, uint8_t* buf, size_t bufLen);
void logs_print_string(FILE* pipe, const char* str);

#endif // LOGS_H
