// Include guard
#ifndef LED_H
#define LED_H

#include "./user_config.h" // PIN_CONN, PIN_PGM, PIN_PVW



// ESP8266
#ifdef ESP8266

#include <gpio.h> // gpio_output_set, GPIO_OUTPUT_SET

// @todo
#define GPIO_SET(pin, state) GPIO_OUTPUT_SET(pin, state)

// Gets PGM pin GPIO mask
#ifdef PIN_PGM
#define _PGM(state) (state<<PIN_PGM)
#else // PIN_PGM
#define _PGM(state) 0
#endif // PIN_PGM

// Gets PVW pin GPIO mask
#ifdef PIN_PVW
#define _PVW(state) (state<<PIN_PVW)
#else // PIN_PVW
#define _PVW(state) 0
#endif // PIN_PVW

// Enables and disables tally LEDs
#if defined(PIN_PGM) || defined(PIN_PVW)
#define _LED_SET(high, low) gpio_output_set(high, low, high | low, 0);
#define LED_TALLY(pgm, pvw) _LED_SET(_PGM(!pgm) | _PVW(!pvw), _PGM(pgm) | _PVW(pvw))
#else // PIN_PGM PIN_PVW
#define LED_TALLY(pgm, pvw)
#endif // PIN_PGM PIN_PVW

#else // ESP8266

#error No GPIO implementation for platform

#endif // ESP8266



// Enables or disables connection LED
#ifndef LED_CONN
#ifdef PIN_CONN
#define LED_CONN(state) GPIO_SET(PIN_CONN, !state)
#else // PIN_CONN
#define LED_CONN(state)
#endif // PIN_CONN
#endif // LED_CONN

#endif // LED_H
