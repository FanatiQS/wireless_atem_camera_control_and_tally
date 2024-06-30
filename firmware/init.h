// Include guard
#ifndef INIT_H
#define INIT_H

// Firmware version
#define FIRMWARE_VERSION_STRING "0.9.0-dev"

// Makes it work with C++ (Arduino)
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Connects to an ATEM switcher and can be configured to
 * send camera control and tally data over SDI using
 * `Blackmagic 3G SDI shield for Arduino` and/or display
 * tally using LEDs.
 * 
 * Features are enabled/disabled and
 * pinouts are configured in `user_config.h`.
 * 
 * It connects directly to LwIP and therefore does not
 * require any ATEM specific polling.
 */
void waccat_init(void);

// Ends extern C block
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // INIT_H
