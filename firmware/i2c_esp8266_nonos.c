#include <stdbool.h> // true

#include <twi.h> // twi_init, twi_readFrom, twi_writeTo

#include "./user_config.h" // PIN_SDA, PIN_SCL
#include "./sdi.h" // SDI_ENABLED
#include "./i2c.h" // SDI_I2C_ADDR

// Uses TWI library for I2C communication on ESP8266
#ifndef ARDUINO
#error ESP8266 I2C implementation relies on arduino libraries
#endif // ARDUINO

// Only compiles I2C function when SDI is enabled
#ifdef SDI_ENABLED

// Initializes I2C device communication
void i2c_init(void) {
	twi_init(PIN_SDA, PIN_SCL);
}

// Writes buffer to I2C device
void i2c_write(uint8_t* write_buf, uint8_t write_len) {
	twi_writeTo(SDI_I2C_ADDR, write_buf, write_len, true);
}

// Writes buffer to and reads response from I2C device
void i2c_write_read(uint8_t* write_buf, uint8_t write_len, uint8_t* read_buf, uint8_t read_len) {
	i2c_write(write_buf, write_len);
	twi_readFrom(SDI_I2C_ADDR, read_buf, read_len, true);
}

#endif // SDI_ENABLED
