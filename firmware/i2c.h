#ifndef I2C_H
#define I2C_H

#include <stdbool.h> // true

#include "./user_config.h" // SDI_I2C_ADDR

// I2C address the SDI shield uses by default
#ifndef SDI_I2C_ADDR
#define SDI_I2C_ADDR 0x6E
#endif // SDI_I2C_ADDR



#ifdef ESP8266

#include <twi.h> // twi_init, twi_readFrom, twi_writeTo

// Uses TWI library for I2C communication on ESP8266
#define I2C_INIT(scl, sda) twi_init(sda, scl)
#define I2C_READ(buf, len) twi_readFrom(SDI_I2C_ADDR, buf, len, true)
#define I2C_WRITE(buf, len) twi_writeTo(SDI_I2C_ADDR, buf, len, true)

#endif // ESP8266

#endif // I2C_H
