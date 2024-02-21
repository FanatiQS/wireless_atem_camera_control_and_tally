// Include guard
#ifndef SDI_H
#define SDI_H

#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool

#include "./user_config.h" // PIN_SCL, PIN_SDA

// Macro to know if SDI is enabled or not
#if defined(PIN_SCL) && defined(PIN_SDA)
#define SDI_ENABLED
// Throws compilation error on invalid I2C pin definitions
#elif defined(PIN_SCL) || defined(PIN_SDA)
#error Both PIN_SCL and PIN_SDA has to be defined if SDI shield is to be used or none of them defined if SDI shield is not to be used
#endif

// Strips out SDI communication functions if SDI is disabled
#ifdef SDI_ENABLED
bool sdi_init(uint8_t dest);
void sdi_write_tally(uint8_t dest, bool pgm, bool pvw);
void sdi_write_cc(uint8_t* buf, uint16_t len);
#else // SDI_ENABLED
#define sdi_init(dest) true
#define sdi_write_tally(dest, pgm, pvw)
#define sdi_write_cc(buf, len)
#endif // SDI_ENABLED

#endif // SDI_H
