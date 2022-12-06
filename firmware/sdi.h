// Include guard
#ifndef SDI_H
#define SDI_H

#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool

#include "./user_config.h" // PIN_SCL

// Macro to know if SDI is enabled or not
#ifdef PIN_SCL
#define SDI_ENABLED
#endif // PIN_SCL

// Strips out SDI writers if SDI is disabled
#ifdef SDI_ENABLED
bool sdi_connect(void);
void sdi_write_tally(uint8_t, bool, bool);
void sdi_write_cc(uint8_t*, uint16_t);
#else // SDI_ENABLED
#define sdi_write_tally(dest, pgm, pvw)
#define sdi_write_cc(buf, len)
#endif // SDI_ENABLED

#endif // SDI_H
