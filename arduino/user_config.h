// Include guard
#ifndef WACCAT_CONFIG_H
#define WACCAT_CONFIG_H

/**
 * Documentation for the configuration parameters can be found in `/firmware/user_config.h`.
 * All macros defined in this file are optional.
 * If a pin is not defined, commented out or removed, that feature is going to be disabled.
 * All macros starting with "PIN_" define the GPIO pin to use.
 */

#include <stdint.h> // used but not included in pins_arduino for ESP8266
#include <pins_arduino.h>

// Debugging flags
#define DEBUG             1
#define DEBUG_TALLY       DEBUG
#define DEBUG_CC          0
#define DEBUG_HTTP        DEBUG
#define DEBUG_ATEM        0
#define DEBUG_DNS         DEBUG

// Pins to use for PGM tally, PVW tally and/or ATEM connection indicator LEDs
#ifdef ESP8266
#define PIN_PGM           D5
#define PIN_PVW           D6
#define PIN_CONN          LED_BUILTIN
#define PIN_CONN_INVERTED 1
#endif // ESP8266

/**
 * To use a "Blackmagic 3G SDI shield for arduino" for tally and camera control over SDI,
 * define the SCL and SDA pins to use for I2C communication and install the BMDSDIControl library
 * available for download from their website.
 *
 * If this is enabled without a shield attached, the microcontroller is not going to be able to
 * fully boot.
 *
 * 3.3v microcontroller requires a logic level converter to be able to communicate with the SDI shield.
 */
// #define PIN_SCL           SCL // D1
// #define PIN_SDA           SDA // D2

/**
 * The analog pin use for battery level reading.
 * When enabled, includes battery level monitoring in the HTTP configuration form.
 *
 * The voltage level has to be brought down to a range the microcontroller can handle.
 * This is done using a voltage divider.
 */
// #define PIN_BATTREAD      A0

#endif // WACCAT_CONFIG_H
