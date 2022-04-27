// Include guard
#ifndef ATEM_CONNECTION_H
#define ATEM_CONNECTION_H

#include <stdbool.h> // bool

extern int atemSock;

void setupAtemSocket(const char* addr);
void maintainAtemConnection(const bool socketHasData);
void restartAtemConnection();

// Ends include guard
#endif
