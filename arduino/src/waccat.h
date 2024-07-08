// Include guard
#ifndef WACCAT_H
#define WACCAT_H

// Makes C functions link correctly with C++ (Arduino)
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Connects to an ATEM switcher and can be configured to
 * send camera control and tally data over SDI using
 * `Blackmagic 3G SDI shield for Arduino` and/or display
 * tally using LEDs.
 * 
 * Configuration like enabled/disabled features and
 * defining pinouts are done in `waccat_config.h`.
 * 
 * It connects directly to LwIP and therefore does not
 * require any polling.
 */
void waccat_init(void);

// Ends extern C block
#ifdef __cplusplus
}
#endif // __cplusplus

#endif // WACCAT_H
