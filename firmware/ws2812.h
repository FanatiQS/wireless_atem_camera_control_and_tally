// Include guard
#ifndef WS2812_H
#define WS2812_H

#include <stdbool.h> // bool

#include "./user_config.h" // TALLY_RGB_PIN

// Strips out RGB LED communication if pin is not defined
#ifdef TALLY_RGB_PIN
bool ws2812_init(void);
bool ws2812_update(bool pgm, bool pvw);
#else // TALLY_RGB_PIN
#define ws2812_init()
#define ws2812_update(pgm, pvw)
#endif // TALLY_RGB_PIN

#endif // WS2812_H
