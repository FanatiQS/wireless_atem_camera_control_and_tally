#include <stdbool.h> // bool, true, false
#include <assert.h> // assert
#include <stddef.h> // NULL

#include <hal/adc_types.h> // ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_BITWIDTH_DEFAULT
#include <esp_adc/adc_oneshot.h> // adc_oneshot_unit_handle_t, adc_oneshot_unit_init_cfg_t, adc_oneshot_new_unit, adc_oneshot_config_channel, adc_oneshot_chan_cfg_t, adc_oneshot_read
#include <esp_adc/adc_cali.h> // adc_cali_raw_to_voltage, adc_cali_handle_t
#include <esp_adc/adc_cali_scheme.h> // adc_cali_curve_fitting_config_t, adc_cali_create_scheme_curve_fitting, adc_cali_line_fitting_config_t, adc_cali_create_scheme_line_fitting, ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED, ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
#include <driver/temperature_sensor.h> // temperature_sensor_handle_t, temperature_sensor_get_celsius, temperature_sensor_config_t, TEMPERATURE_SENSOR_CONFIG_DEFAULT, temperature_sensor_install, temperature_sensor_enable
#include <esp_err.h> // esp_err_t, ESP_OK, ESP_ERROR_CHECK, esp_err_to_name

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_PRINTF
#include "./user_config.h" // PIN_BATTREAD, VOLTAGE_READ_R1, VOLTAGE_READ_R2
#include "./sensors.h"



// Voltage ADC handles
static adc_oneshot_unit_handle_t sensors_voltage_handle;
static adc_cali_handle_t sensors_voltage_cali_handle;

// Internal temperature sensor handle
static temperature_sensor_handle_t sensors_temp_handle;



// Initializes voltage and temperature sensors
void sensors_init(void) {
	esp_err_t err;

	// Creates ADC for voltage reading
	adc_oneshot_unit_init_cfg_t config_voltage_init = {
		.unit_id = ADC_UNIT_1
	};
	err = adc_oneshot_new_unit(&config_voltage_init, &sensors_voltage_handle);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to create ADC handle: %s\n", esp_err_to_name(err));
	}
	else {
		// Configures ADC voltage reader
		adc_oneshot_chan_cfg_t config_voltage_chan = {
			.atten = ADC_ATTEN_DB_12,
			.bitwidth = ADC_BITWIDTH_DEFAULT
		};
		ESP_ERROR_CHECK(adc_oneshot_config_channel(sensors_voltage_handle, PIN_BATTREAD, &config_voltage_chan));
	}

	// Creates curve-fitting calibration
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	adc_cali_curve_fitting_config_t config_voltage_cali_curve = {
		.unit_id = ADC_UNIT_1,
		.chan = PIN_BATTREAD,
		.atten = ADC_ATTEN_DB_12,
		.bitwidth = ADC_BITWIDTH_DEFAULT
	};
	err = adc_cali_create_scheme_curve_fitting(&config_voltage_cali_curve, &sensors_voltage_cali_handle);
	if (err == ESP_OK) {
		DEBUG_PRINTF("Calibration scheme used for voltage reading: Curve Fitting\n");
	}
	else {
		DEBUG_ERR_PRINTF("Failed to calibrate voltage reader with curve-fitting: %s\n", esp_err_to_name(err));
#endif // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	// Falls back to line-fitting calibration if curve-fitting is not available or failed
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
		adc_cali_line_fitting_config_t config_voltage_cali_line = {
			.unit_id = ADC_UNIT_1,
			.atten = ADC_ATTEN_DB_12,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		err = adc_cali_create_scheme_line_fitting(&config_voltage_cali_line, &sensors_voltage_cali_handle);
		if (err == ESP_OK) {
			DEBUG_PRINTF("Calibration scheme used for voltage reading: Line Fitting\n");
		}
		else {
			DEBUG_ERR_PRINTF("Failed to calibrate voltage reader with line-fitting: %s\n", esp_err_to_name(err));
#endif // ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
	DEBUG_PRINTF("No voltage calibration used\n");
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
		}
#endif // ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
	}
#endif // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED

	// Initializes internal temperature sensor
	temperature_sensor_config_t config_temp = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
	ESP_ERROR_CHECK(temperature_sensor_install(&config_temp, &sensors_temp_handle));
	ESP_ERROR_CHECK(temperature_sensor_enable(sensors_temp_handle));
}

// Reads input voltage before voltage divider
bool sensors_voltage_read(float* voltage) {
	assert(voltage != NULL);

	// Reads raw analog value
	int adc_raw;
	esp_err_t err = adc_oneshot_read(sensors_voltage_handle, PIN_BATTREAD, &adc_raw);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read ADC data: %s\n", esp_err_to_name(err));
		return false;
	}

	// Converts raw ADC data to millivolt voltage level at MCU
	int adc_cali;
	err = adc_cali_raw_to_voltage(sensors_voltage_cali_handle, adc_raw, &adc_cali);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to calibrate raw ADC data: %s\n", esp_err_to_name(err));
		return false;
	}

	// Calculates voltage value before passed through voltage divider
	*voltage = (float)adc_cali * (VOLTAGE_READ_R1 + VOLTAGE_READ_R2) / VOLTAGE_READ_R2 / 1000;

	return true;
}

// Reads chips internal temperature
bool sensors_temperature_read(float* temp) {
	assert(temp != NULL);
	esp_err_t err = temperature_sensor_get_celsius(sensors_temp_handle, temp);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to read temperature: %s\n", esp_err_to_name(err));
		return false;
	}
	return true;
}
