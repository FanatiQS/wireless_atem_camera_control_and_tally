// Include guard
#ifndef USER_CONFIG_H
#define USER_CONFIG_H

#ifdef NO_CONFIG_INCLUDE
#include <waccat_config.h>
#endif // NO_CONFIG_INCLUDE

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

#endif // USER_CONFIG_H
