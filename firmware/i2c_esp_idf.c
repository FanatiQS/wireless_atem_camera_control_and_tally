#include <stdbool.h> // true
#include <stdint.h> // uint8_t

#include <driver/i2c_master.h> // i2c_master_bus_config_t, i2c_new_master_bus, i2c_device_config_t, i2c_master_bus_add_device, i2c_master_transmit, i2c_master_transmit_receive
#include <driver/i2c_types.h> // i2c_master_dev_handle_t, i2c_master_bus_handle_t, I2C_NUM_0, I2C_ADDR_BIT_LEN_7, I2C_CLK_SRC_DEFAULT
#include <esp_err.h> // esp_err_t, ESP_OK, ESP_ERR_TIMEOUT, ESP_ERROR_CHECK, esp_err_to_name

#include "./user_config.h" // PIN_SCL, PIN_SDA
#include "./debug.h" // DEBUG_ERR_PRINTF
#include "./sdi.h" // SDI_ENABLED
#include "./i2c.h" // SDI_I2C_ADDR



// Only compiles I2C function when SDI is enabled
#ifdef SDI_ENABLED

// I2C device to use for reading/writing
static i2c_master_dev_handle_t i2c_device;

// Initializes I2C device communication
void i2c_init(void) {
	// Initializes I2C bus
	i2c_master_bus_handle_t i2c_bus;
	i2c_master_bus_config_t config_bus = {
		.i2c_port = I2C_NUM_0,
		.sda_io_num = PIN_SDA,
		.scl_io_num = PIN_SCL,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&config_bus, &i2c_bus));

	// Initializes I2C device and registers bus
	i2c_device_config_t config_device = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = SDI_I2C_ADDR,
		.scl_speed_hz = 100000,
	};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &config_device, &i2c_device));
}

// Writes buffer to I2C device
void i2c_write(uint8_t* write_buf, uint8_t write_len) {
    esp_err_t err = i2c_master_transmit(i2c_device, write_buf, write_len, -1);
	if (err == ESP_OK) return;
	DEBUG_ERR_PRINTF("Failed to write I2C data: %s\n", esp_err_to_name(err));
}

// Writes buffer to and reads response from I2C device
void i2c_write_read(uint8_t* write_buf, uint8_t write_len, uint8_t* read_buf, uint8_t read_len) {
	esp_err_t err = i2c_master_transmit_receive(i2c_device, write_buf, write_len, read_buf, read_len, -1);
	if (err == ESP_OK) return;
	DEBUG_ERR_PRINTF("Failed to write and read I2C data: %s\n", esp_err_to_name(err));
}

#endif // SDI_ENABLED
