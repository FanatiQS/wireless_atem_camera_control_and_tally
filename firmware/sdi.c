// Only defines SDI functions if SDI shield is to be used
#include "./sdi.h" // SDI_ENABLED
#ifdef SDI_ENABLED

#include <stdint.h> // uint8_t, uint16_t, uint32_t
#include <stdbool.h> // bool, true, false

#include <lwip/sys.h> // sys_now

#include "./user_config.h" // PIN_SCL, PIN_SDA, DEBUG, SDI_INIT_TIMEOUT
#include "./debug.h" // DEBUG_PRINTF, DEBUG_ERR_PRINTF
#include "./i2c.h" // I2C_INIT, I2C_READ, I2C_WRITE

// Number of milliseconds to wait for SDI shield FPGA to get ready before failing
#ifndef SDI_INIT_TIMEOUT
#define SDI_INIT_TIMEOUT 2000
#endif // SDI_INIT_TIMEOUT



// SDI shield registers
#define kRegIDENTIFIER 0x0000
#define kRegFWVERSION  0x0004
#define kRegPVERSION   0x0006
#define kRegCONTROL    0x1000
#define kRegOCARM      0x2000
#define kRegOCLENGTH   0x2001
#define kRegOCDATA     0x2100
#define kRegOTARM      0x4000
#define kRegOTLENGTH   0x4001
#define kRegOTDATA     0x4100

// SDI shield override masks
#define kRegCONTROL_COVERIDE_Mask 0x01
#define kRegCONTROL_TOVERIDE_Mask 0x02

// Writes variadic number of bytes to SDI shield register
#define SDI_WRITE_BUF(buf) I2C_WRITE(buf, sizeof(buf) / sizeof(buf[0]))
#define SDI_WRITE(reg, ...) SDI_WRITE_BUF(((uint8_t[]){ (reg) & 0xff, (reg) >> 8, __VA_ARGS__ }))

// Reads SDI shield data from registers to buffer
static void sdi_read(uint16_t reg, uint8_t* readBuf, uint8_t readLen) {
	SDI_WRITE_BUF(((uint8_t[]){ reg & 0xff, reg >> 8 }));
	I2C_READ(readBuf, readLen);
}

// Prints an SDI version type read from shield
#if DEBUG
static void sdi_version_print(const char* label, uint16_t reg) {
	uint8_t buf[2];
	sdi_read(reg, buf, sizeof(buf));
	DEBUG_PRINTF("SDI shield %s version: %d.%d\n", label, buf[1], buf[0]);
}
#else // DEBUG
#define sdi_version_print(label, reg)
#endif // DEBUG



// Checks if SDI shield FPGA has booted
static bool sdi_connect(void) {
	uint32_t buf;
	sdi_read(kRegIDENTIFIER, (uint8_t*)&buf, sizeof(buf));
	return buf == *(uint32_t*)"SDIC";
}

// Tries to connect to the SDI shield
bool sdi_init(uint8_t dest) {
	// Initializes I2C driver
	I2C_INIT(PIN_SCL, PIN_SDA);

	// Awaits SDI shields FPGA booting up
	while (!sdi_connect()) {
		if (sys_now() > SDI_INIT_TIMEOUT) {
			DEBUG_ERR_PRINTF("Failed to connect to SDI shield\n");
			return false;
		}
	}

	// Enables overwriting tally and camera control data in SDI shield
	SDI_WRITE(kRegCONTROL, kRegCONTROL_COVERIDE_Mask | kRegCONTROL_TOVERIDE_Mask);

	// Sets tally data length in SDI shield register
	SDI_WRITE(kRegOTLENGTH, dest, 0);

	// Prints SDI shields internal firmware and protocol version
	sdi_version_print("firmware", kRegFWVERSION);
	sdi_version_print("protocol", kRegPVERSION);

	return true;
}



// Awaits SDI shield override bank to be ready for write
static void sdi_flush(uint16_t reg) {
	uint8_t busy;
	do {
		sdi_read(reg, &busy, sizeof(busy));
	} while (busy);
}

// Writes tally data to the SDI shield
void sdi_write_tally(uint8_t dest, bool pgm, bool pvw) {
	sdi_flush(kRegOTARM);
	SDI_WRITE(kRegOTDATA + dest - 1, pgm | (uint8_t)(pvw << 1));
	SDI_WRITE(kRegOTARM, true);
}

// Writes camera control data to the SDI shield, requires 2 header bytes for register address
void sdi_write_cc(uint8_t* buf, uint16_t len) {
	sdi_flush(kRegOCARM);
	SDI_WRITE(kRegOCLENGTH, len & 0xff, len >> 8);
	buf[0] = kRegOCDATA & 0xff;
	buf[1] = kRegOCDATA >> 8;
	I2C_WRITE(buf, len + 2);
	SDI_WRITE(kRegOCARM, true);
}

#endif // SDI_ENABLE
