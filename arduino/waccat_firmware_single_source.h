// Include guard
#ifndef WACCAT_SINGLE_SOURCE_H
#define WACCAT_SINGLE_SOURCE_H

/*
 * Include this file in a .c file locally in your project.
 * C++ does not work as C++ still does not support designated initializers from C99 correctly.
 * Configurations can be put before the include.
 */

// Includes all required source files
#include "../core/atem.c"
#include "../firmware/atem_sock.c"
#include "../firmware/dns.c"
#include "../firmware/flash.c"
#include "../firmware/http_respond.h"
#include "../firmware/http.c"
#include "../firmware/init.c"
#include "../firmware/sdi.c"

#endif // WACCAT_SINGLE_SOURCE_H
