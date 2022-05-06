// Include guard
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#define DEBUG
#define DEBUG_TALLY
// #define DEBUG_CC

#define PIN_PGM D5
#define PIN_PVW D6
#define PIN_CONN LED_BUILTIN

// #define PIN_SCL D2
// #define PIN_SDA D1

/**
 * Enables or disables battery level monitoring in HTTP configuration form
 * Define USE_BATTREAD as a macro to enable battery level monitoring in HTTP.
 *
 * Sets pin to use for analog read of voltage level.
 *
 * The voltage level has to be brought down to a range the microcontroller can handle.
 * This is done using a voltage divider.
 */
// #define PIN_BATTREAD A0

// End include guard
#endif
