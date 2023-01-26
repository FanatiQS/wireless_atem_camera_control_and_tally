#include <stdint.h> // uint8_t, uint16_t
#include <stdbool.h> // bool, true, false

#include <lwip/arch.h> // sys_now
#include <twi.h> // twi_init, twi_readFrom, twi_writeTo

#include "./user_config.h" // PIN_SCL, PIN_SDA, DEBUG
#include "./debug.h" // DEBUG_PRINTF
#include "./sdi.h" // SDI_ENABLED

// Throws compilation error on invalid I2C pin definitions
#if !(defined(PIN_SCL) && defined(PIN_SDA)) && (defined(PIN_SCL) || defined(PIN_SDA))
#error Both PIN_SCL and PIN_SDA has to be defined if SDI shield is to be used or none of them defined if SDI shield is not to be used
#endif

// Number of milliseconds to wait for SDI shield FPGA to get ready
#ifndef SDI_INIT_TIMEOUT
#define SDI_INIT_TIMEOUT 2000
#endif // SDI_INIT_TIMEOUT

// I2C address the SDI shield uses by default
#ifndef SDI_I2C_ADDR
#define SDI_I2C_ADDR 0x6E
#endif // SDI_I2C_ADDR

// Uses TWI library for I2C communication
#define I2C_INIT(scl, sda) twi_init(sda, scl)
#define I2C_READ(buf, len) twi_readFrom(SDI_I2C_ADDR, buf, len, true)
#define I2C_WRITE(buf, len) twi_writeTo(SDI_I2C_ADDR, buf, len, true)



// SDI shield registers
#define kRegIDENTIFIER 0x0000
#define kRegFWVERSION  0x0004
#define kRegPVERSION   0x0006
#define kRegCONTROL    0x1000

// SDI shield override masks
#define kRegCONTROL_COVERIDE_Mask 0x01
#define kRegCONTROL_TOVERIDE_Mask 0x02

// Prints an SDI version type read from shield
#if DEBUG
#define SDI_VERSION_PRINT(label, reg)\
	do {\
		uint8_t buf[2];\
		sdi_read(reg, buf, sizeof(buf));\
		DEBUG_PRINTF("SDI shield " label " version: %d.%d\n", buf[1], buf[0]);\
	} while (0)
#else // DEBUG
#define SDI_VERSION_PRINT(reg, label)
#endif // DEBUG

// Writes variadic number of bytes to SDI shield register
#define _SDI_WRITE(buf) I2C_WRITE(buf, sizeof(buf) / sizeof(buf[0]))
#define SDI_WRITE(addr, ...) _SDI_WRITE(((uint8_t[]){ addr & 0xff, addr >> 8, __VA_ARGS__ }))

// Only defines SDI functions if SDI shield is to be used
#ifdef SDI_ENABLED

// Reads SDI shield data from registers to buffer
static void sdi_read(uint16_t addr, uint8_t* readBuf, uint8_t readLen) {
	SDI_WRITE(addr);
	I2C_READ(readBuf, readLen);
}



// Checks if SDI shield FPGA has booted
static bool sdi_connect() {
	uint8_t buf[4];
	sdi_read(kRegIDENTIFIER, buf, 4);
	return (buf[0] == 'S') && (buf[1] == 'D') && (buf[2] == 'I') && (buf[3] == 'C');
}

// Tries to connect to the SDI shield
bool sdi_init() {
	// Initializes I2C driver
	I2C_INIT(PIN_SCL, PIN_SDA);

	// Awaits SDI shields FPGA booting up
	while (!sdi_connect()) {
		if (sys_now() > SDI_INIT_TIMEOUT) {
			DEBUG_PRINTF("Failed to connect to SDI shield\n");
			return false;
		}
	}

	// Enables overwriting tally and camera control data in SDI shield
	uint8_t ctrl;
	sdi_read(kRegCONTROL, &ctrl, 1);
	ctrl |= (kRegCONTROL_COVERIDE_Mask | kRegCONTROL_TOVERIDE_Mask);
	SDI_WRITE(kRegCONTROL, ctrl);

	// Prints SDI shields internal firmware and protocol version
	SDI_VERSION_PRINT("firmware", kRegFWVERSION);
	SDI_VERSION_PRINT("protocol", kRegPVERSION);

	return true;
}



// @todo tally and camera control still uses the bad arduino library
void _sdi_write_tally(uint8_t, bool, bool);
void _sdi_write_cc(uint8_t*, uint16_t);

// Writes tally data to the SDI shield
void sdi_write_tally(uint8_t dest, bool pgm, bool pvw) {
	_sdi_write_tally(dest, pgm, pvw);
}

// Writes camera control data to the SDI shield
void sdi_write_cc(uint8_t* buf, uint16_t len) {
	_sdi_write_cc(buf, len);
}

#endif // SDI_ENABLE
