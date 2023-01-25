#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool, true, false

#include <lwip/arch.h> // sys_now

#include "./user_config.h" // PIN_SCL, PIN_SDA
#include "./debug.h" // DEBUG_PRINTF
#include "./sdi.h" // SDI_ENABLED

// Throws compilation error on invalid I2C pin definitions
#if !(defined(PIN_SCL) && defined(PIN_SDA)) && (defined(PIN_SCL) || defined(PIN_SDA))
#error Both PIN_SCL and PIN_SDA has to be defined if SDI shield is to be used or none of them defined if SDI shield is not to be used
#endif

#ifndef SDI_I2C_ADDR
#define SDI_I2C_ADDR 0x6E
#endif // SDI_I2C_ADDR

#ifdef SDI_ENABLED

// @todo currently it still uses the bad arduino library
bool _sdi_connect(void);
void _sdi_write_tally(uint8_t, bool, bool);
void _sdi_write_cc(uint8_t*, uint16_t);

// Tries to connect to the SDI shield
bool sdi_init() {
	while (sys_now() < 2000) {
		if (_sdi_connect()) {
			DEBUG_PRINTF("Connected to SDI shield\n");
			return true;
		}
	}

	DEBUG_PRINTF("Failed to connect to SDI shield\n");
	return false;
}

// Writes tally data to the SDI shield
void sdi_write_tally(uint8_t dest, bool pgm, bool pvw) {
	_sdi_write_tally(dest, pgm, pvw);
}

// Writes camera control data to the SDI shield
void sdi_write_cc(uint8_t* buf, uint16_t len) {
	_sdi_write_cc(buf, len);
}

#endif // SDI_ENABLE
