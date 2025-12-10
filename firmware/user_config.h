// Include guard
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// Includes users actual configuration file unless that is handled manually by defining NO_CONFIG_INCLUDE
#ifndef NO_CONFIG_INCLUDE
#include <waccat_config.h>
#endif // NO_CONFIG_INCLUDE

// Checks for duplicate pin definitions at compile time
#define CHECK_DUPLICATE_PIN_MAKE_NAME(pin) PIN_WAS_DEFINED_MORE_THAN_ONCE_USING_VALUE_ ## pin
#define CHECK_DUPLICATE_PIN(pin) enum { CHECK_DUPLICATE_PIN_MAKE_NAME(pin) }

/**
 * @def DEBUG
 * @brief Enables or disables base level logging and is required for other log levels.
 *
 * When enabled, boot logs and errors will be printed to serial.
 * This flag is required for any of the `DEBUG_*` flags.
 *
 * - Set to `1` to enable base logging.
 * - Set to `0` to disable base logging.
 *
 * **Default value:** `0`
 */
#if defined(DEBUG) && DEBUG != 0 && DEBUG != 1
#error Invalid configuration value for DEBUG
#endif // DEBUG != 0 && DEBUG != 1

/**
 * @def DEBUG_TALLY
 * @brief Enables or disables tally state logging.
 *
 * When enabled, tally state is logged to serial when changed.
 * Requires `DEBUG` flag to be enabled.
 *
 * - Set to `1` to enable tally logging.
 * - Set to `0` to disable tally logging.
 *
 * **Default value:** `0`
 */
#if DEBUG_TALLY
#if !DEBUG
#error Enabling tally debugging requires general debugging to be enabled
#endif // !DEBUG
#if DEBUG_TALLY != 0 && DEBUG_TALLY != 1
#error Invalid configuration value for DEBUG_TALLY
#endif // DEBUG_TALLY != 0 && DEBUG_TALLY != 1
#endif // DEBUG_TALLY

/**
 * @def DEBUG_CC
 * @brief Enables or disables camera control logging.
 * @attention Should only be enabled if required since the amount of
 * camera control data logged can severely slow down the device.
 *
 * When enabled, camera control data for selected camera id is logged to
 * serial.
 *
 * - Set to `1` to enable camera control logging.
 * - Set to `0` to disable camera control logging.
 *
 * **Default value:** `0`
 */
#if DEBUG_CC
#if !DEBUG
#error Enabling camera control debugging requires general debugging to be enabled
#endif // !DEBUG
#if DEBUG_CC != 0 && DEBUG_CC != 1
#error Invalid configuration value for DEBUG_CC
#endif // DEBUG_CC != 0 && DEBUG_CC != 1
#endif // DEBUG_CC

/**
 * @def DEBUG_ATEM
 * @brief Enables or disables ATEM logging.
 * @attention Should only be enabled if required since the amount of
 * ATEM packets received can be enough to severely slow down the device.
 *
 * When enabled, the remote id of all received ATEM packets are logged to serial,
 * including if they are received out of order or not.
 *
 * - Set to `1` to enable ATEM packet ID logging.
 * - Set to `0` to disable ATEM packet ID logging.
 *
 * **Default value:** `0`
 */
#if DEBUG_ATEM
#if !DEBUG
#error Enabling ATEM protocol debugging requires general debugging to be enabled
#endif // !DEBUG
#if DEBUG_ATEM != 0 && DEBUG_ATEM != 1
#error Invalid configuration value for DEBUG_ATEM
#endif // DEBUG_ATEM != 0 && DEBUG_ATEM != 1
#endif // DEBUG_ATEM

/**
 * @def DEBUG_HTTP
 * @brief Enables or disables HTTP logging.
 *
 * When enabled, HTTP events are logged to serial.
 *
 * - Set to `1` to enable HTTP logging.
 * - Set to `0` to disable HTTP logging.
 *
 * **Default value:** `0`
 */
#if DEBUG_HTTP
#if !DEBUG
#error Enabling HTTP debugging requires general debugging to be enabled
#endif // !DEBUG
#if DEBUG_HTTP != 0 && DEBUG_HTTP != 1
#error Invalid configuration value for DEBUG_HTTP
#endif // DEBUG_HTTP != 0 && DEBUG_HTTP != 1
#endif // DEBUG_HTTP

/**
 * @def DEBUG_DNS
 * @brief Enables or disables captive portal DNS logging.
 *
 * When enabled, captive portal DNS events are logged to serial.
 *
 * - Set to `1` to enable captive portal DNS logging.
 * - Set to `0` to disable captive portal DNS logging.
 *
 * **Default value:** `0`
 */
#if DEBUG_DNS
#if !DEBUG
#error Enabling DNS debugging requires general debugging to be enabled
#endif // !DEBUG
#if DEBUG_DNS != 0 && DEBUG_DNS != 1
#error Invalid configuration value for DEBUG_DNS
#endif // DEBUG_DNS != 0 && DEBUG_DNS != 1
#endif // DEBUG_DNS



/**
 * @def PIN_PGM
 * @brief Sets pin number to use when outputting program tally to LED.
 * 
 * Program tally LED is disabled when this macro is not defined.
 * 
 * Valid numbers are any positive integer supported by the platform.
 */
#if defined(PIN_PGM) && PIN_PGM >= 0
CHECK_DUPLICATE_PIN(PIN_PGM);
#endif // PIN_PGM && PIN_PGM >= 0

/**
 * @def PIN_PVW
 * @brief Sets pin number to use when outputting preview tally to LED.
 *
 * Preview tally LED is disabled when this macro is not defined.
 *
 * Valid numbers are any positive integer supported by the platform.
 */
#if defined(PIN_PVW) && PIN_PVW >= 0
CHECK_DUPLICATE_PIN(PIN_PVW);
#endif // PIN_PVW && PIN_PVW >= 0

/**
 * @def PIN_CONN
 * @brief Sets pin number to use when outputting ATEM connection status to LED.
 *
 * ATEM connection status LED is disabled when this macro is not defined.
 *
 * Valid numbers are any positive integers supported by the platform.
 */
#if defined(PIN_CONN) && PIN_CONN >= 0
CHECK_DUPLICATE_PIN(PIN_CONN);
#endif // PIN_CONN && PIN_CONN >= 0

/**
 * @def PIN_TALLY_RGB
 * @brief Sets pin number to use when outputting tally to RGB LED matrix.
 *
 * Tally RGB LED is disabled when this macro is not defined.
 *
 * Valid numbers are any positive integers.
 */
#if defined(PIN_TALLY_RGB) && PIN_TALLY_RGB >= 0
CHECK_DUPLICATE_PIN(PIN_TALLY_RGB);
#endif // PIN_TALLY_RGB && PIN_TALLY_RGB >= 0

/**
 * @def PIN_SCL
 * @brief Sets pin number to use for I2C SCL.
 *
 * This macro requires PIN_SDA to also be defined.
 * I2C is disabled when this macro and PIN_SDA are not defined.
 *
 * Valid numbers are any positive integers supported by the platform.
 */
#if defined(PIN_SCL) && PIN_SCL >= 0
CHECK_DUPLICATE_PIN(PIN_SCL);
#endif // PIN_SCL && PIN_SCL >= 0


/**
 * @def PIN_SDA
 * @brief Sets pin number to use for I2C SDA.
 *
 * This macro requires PIN_SCL to also be defined.
 * I2C is disabled when this macro and PIN_SCL are not defined.
 *
 * Valid numbers are any positive integers supported by the platform.
 */
#if defined(PIN_SDA) && PIN_SDA >= 0
CHECK_DUPLICATE_PIN(PIN_SDA);
#endif // PIN_SDA && PIN_SDA >= 0

/**
 * @def PIN_BATTREAD
 * @brief Sets pin number to use when reading battery voltage.
 *
 * Battery voltage reading is disabled when this macro is not defined.
 *
 * Valid numbers are any positive integers supported by the platform.
 */
#if defined(PIN_BATTREAD) && PIN_BATTREAD >= 0
CHECK_DUPLICATE_PIN(PIN_BATTREAD);
#endif // PIN_BATTREAD && PIN_BATTREAD >= 0

#endif // USER_CONFIG_H
