// Include guard
#ifndef I2C_H
#define I2C_H

#include <stdint.h> // uint8_t

#include "./user_config.h" // SDI_I2C_ADDR

// I2C address the SDI shield uses by default
#ifndef SDI_I2C_ADDR
#define SDI_I2C_ADDR (0x6E)
#endif // SDI_I2C_ADDR

// Platform implemented I2C functions
void i2c_init(void);
void i2c_write(uint8_t* write_buf, uint8_t write_len);
void i2c_write_read(uint8_t* write_buf, uint8_t write_len, uint8_t* read_buf, uint8_t read_len);

#endif // I2C_H
