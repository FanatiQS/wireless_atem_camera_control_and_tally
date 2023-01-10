// Include guard
#ifndef LED_H
#define LED_H

#include "./user_config.h" // PIN_CONN, PIN_PGM, PIN_PVW



// ESP8266
#ifdef ESP8266

// Gets a pointer to the registry address
#define GET_REG_ADDR(addr) *((volatile uint32_t *)(0x60000000+(addr)))

// Directly modifies register addresses
#define GPIO_INIT(mask) (GET_REG_ADDR(0x310) = mask)
#define GPIO_SET(mask) (GET_REG_ADDR(0x304) = mask)
#define GPIO_CLR(mask) (GET_REG_ADDR(0x308) = mask)

#else // ESP8266

#error No GPIO implementation for platform

#endif // ESP8266



// Gets CONN pin GPIO mask
#ifdef PIN_CONN
#define CONN(state) (state<<PIN_CONN)
#else // PIN_CONN
#define CONN(state) 0
#endif // PIN_CONN

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



// Initializes all GPIO LEDs
#ifndef LED_INIT
#define LED_INIT() GPIO_INIT(CONN(1) | PGM(1) | PVW(1))
#endif // LED_INIT

// Enables and disables tally LEDs
#ifndef LED_TALLY
#if (defined(PIN_PGM) || defined(PIN_PVW))
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
#define LED_CONN(state) (state) ? (GPIO_CLR(1 << PIN_CONN)) : GPIO_SET(1 << PIN_CONN)
#else // PIN_CONN
#define LED_CONN(state)
#endif // PIN_CONN
#endif // LED_CONN

#endif // LED_H
