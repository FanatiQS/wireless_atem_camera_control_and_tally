// Include guard
#ifndef LED_H
#define LED_H

#include "./user_config.h" // PIN_CONN, PIN_PGM, PIN_PVW



// ESP8266
#ifdef ESP8266

#include <eagle_soc.h> // GPIO_REG_WRITE, GPIO_PIN0_ADDRESS, GPIO_OUT_W1TS_ADDRESS, GPIO_OUT_W1TC_ADDRESS

// Directly modifies register addresses
#define LED_INIT(pin) GPIO_REG_WRITE(GPIO_PIN0_ADDRESS + pin * 4, 0)
#define GPIO_SET(mask) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask)
#define GPIO_CLR(mask) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask)

#else // ESP8266

#error No GPIO implementation for platform

#endif // ESP8266



// Gets PGM pin GPIO mask
#ifdef PIN_PGM
#define PGM(state) (state<<PIN_PGM)
#else // PIN_PGM
#define PGM(state) 0
#endif // PIN_PGM

// Gets PVW pin GPIO mask
#ifdef PIN_PVW
#define PVW(state) (state<<PIN_PVW)
#else // PIN_PVW
#define PVW(state) 0
#endif // PIN_PVW



// Enables and disables tally LEDs
#ifndef LED_TALLY
#if defined(PIN_PGM) || defined(PIN_PVW)
#define LED_TALLY(pgm, pvw) do {\
		GPIO_SET(PGM(pgm) | PVW(pvw));\
		GPIO_CLR(PGM(!pgm) | PVW(!pvw));\
	} while (0)
#else // PIN_PGM || PIN_PVW
#define LED_TALLY(pgm, pvw)
#endif // PIN_PGM || PIN_PVW
#endif // LED_TALLY

// Enables or disables connection LED
#ifndef LED_CONN
#ifdef PIN_CONN
#define LED_CONN(state) if (state) (GPIO_CLR(1 << PIN_CONN)); else GPIO_SET(1 << PIN_CONN)
#else // PIN_CONN
#define LED_CONN(state)
#endif // PIN_CONN
#endif // LED_CONN

#endif // LED_H
