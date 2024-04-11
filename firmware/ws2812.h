// Include guard
#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h> // bool

#include "./user_config.h" // PIN_TALLY_RGB

// Strips out RGB LED communication if pin is not defined
#ifdef PIN_TALLY_RGB
bool ws2812_init(void);
bool ws2812_update(bool pgm, bool pvw);
#else // PIN_TALLY_RGB
#define ws2812_init()
#define ws2812_update(pgm, pvw)
#endif // PIN_TALLY_RGB

#endif // WS2812_H
