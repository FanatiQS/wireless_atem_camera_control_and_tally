// Include guard
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

/**
 * All macros defined in this file are optional.
 * If a pin is not defined, commented out or removed, that feature is going to be disabled.
 * All macros starting with "PIN_" define the GPIO pin to use.
 */

/**
 * Enables or disables debugging.
 * DEBUG is general debugging.
 * DEBUG_TALLY prints tally state when updated.
 * DEBUG_CC prints message when receiving camera control data
 */
#define DEBUG
#define DEBUG_TALLY
// #define DEBUG_CC

/**
 * The pins to use for PGM tally, PVW tally and/or ATEM connection indicator LEDs.
 */
#define PIN_PGM D5
#define PIN_PVW D6
#define PIN_CONN LED_BUILTIN

/**
 * To use a "Blackamgic 3G SDI shield for arduino" for tally and camera control over SDI,
 * define the SCL and SDA pins to use for I2C communication and install the BMDSDIControl library
 * available for download from their website.
 *
 * If this is enabled without a shield attached, the microcontroller is never going to be able to
 * initialize and is most likely crash.
 *
 * 3.3v microcontroller requires a logic level converter to be able to communicate with the SDI shield.
 */
// #define PIN_SCL D2
// #define PIN_SDA D1

/**
 * The analog pin use for battery level reading.
 * When enabled, includes battery level monitoring in the HTTP configuration form.
 *
 * The voltage level has to be brought down to a range the microcontroller can handle.
 * This is done using a voltage divider.
 */
// #define PIN_BATTREAD A0

// End include guard
#endif
