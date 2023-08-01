// Include guard
#ifndef WACCAT_SINGLE_SOURCE_H
#define WACCAT_SINGLE_SOURCE_H

/*
 * Include this file in a .c file locally in your project.
 * Configurations can be put before the include.
 */

// Includes all required source files
#include "./src/atem.c"
#include "./firmware/atem_sock.c"
#include "./firmware/dns.c"
#include "./firmware/flash.c"
#include "./firmware/http.c"
#include "./firmware/init.c"
#include "./firmware/main.c"
#include "./firmware/sdi.c"

// Ends include guard
#endif // WACCAT_SINGLE_SOURCE_H
