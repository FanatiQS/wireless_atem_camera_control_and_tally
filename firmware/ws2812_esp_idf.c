// Only defines RGB LED functions if configured to be used
#include "./user_config.h" // PIN_TALLY_RGB, PIN_TALLY_RGB_COUNT
#ifdef PIN_TALLY_RGB

#include <stdbool.h> // bool, false, true
#include <stdint.h> // uint8_t

#include <esp_err.h> // esp_err_t, ESP_OK
#include <driver/rmt_tx.h> // rmt_tx_channel_config_t, rmt_new_tx_channel, rmt_transmit, rmt_tx_wait_all_done, rmt_transmit_config_t
#include <driver/rmt_types.h> // rmt_channel_handle_t, rmt_encoder_handle_t, RMT_CLK_SRC_DEFAULT
#include <driver/rmt_common.h> // rmt_enable
#include <driver/rmt_encoder.h> // rmt_bytes_encoder_config_t, rmt_new_bytes_encoder
#include <rom/ets_sys.h> // ets_delay_us
#include <freertos/portmacro.h> // portMAX_DELAY

#include "./debug.h" // DEBUG_ERR_PRINTF
#include "./ws2812.h"

#ifndef PIN_TALLY_RGB_COUNT
#error RGB tally was enabled without specifying number of LEDs using PIN_TALLY_RGB_COUNT
#endif // PIN_TALLY_RGB_COUNT

#define TALLY_RGB_BRIGHTNESS 0x8 // @todo replace with runtime configurable value from config

// Defaults to only RGB instead of RGBW (untested)
#ifndef TALLY_RGB_RGBW
#define TALLY_RGB_RGBW (false)
#endif // TALLY_RGB_RGBW

// Defaults to Green,Red,Blue color order like Adafruits NeoPixel library
#if !defined(TALLY_RGB_OFFSET_RED) || !defined(TALLY_RGB_OFFSET_GREEN)
#define TALLY_RGB_OFFSET_RED   (1)
#define TALLY_RGB_OFFSET_GREEN (0)
#endif // TALLY_RGB_OFFSET_RED || TALLY_RGB_OFFEST_GREEN

// Nanosecond timings for ws2812 RGB LED matrix
#define LED_RMT_T0H_NS   (300)
#define LED_RMT_T0L_NS   (900)
#define LED_RMT_T1H_NS   (900)
#define LED_RMT_T1L_NS   (300)
#define LED_RMT_RESET_US (280)

// RMT resolution of 10MHz
#define LED_RMT_FREQUENCY (10000000)



// RGB LED matrix instance
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;

// Updates tally RGB LED matrix
bool ws2812_update(bool pgm, bool pvw) {
	esp_err_t err;

	// Creates RGB LED buffer
	uint8_t buf[PIN_TALLY_RGB_COUNT * (3 + TALLY_RGB_RGBW)] = {0};
	if (pgm) {
		for (int i = 0; i < PIN_TALLY_RGB_COUNT; i++) {
			buf[i * 3 + TALLY_RGB_OFFSET_RED] = TALLY_RGB_BRIGHTNESS;
		}
	}
	else if (pvw) {
		for (int i = 0; i < PIN_TALLY_RGB_COUNT; i++) {
			buf[i * 3 + TALLY_RGB_OFFSET_GREEN] = TALLY_RGB_BRIGHTNESS;
		}
	}

	// Writes RGB LED buffer
	rmt_transmit_config_t config = {0};
	err = rmt_transmit(channel, encoder, buf, sizeof(buf), &config);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to submit RMT data: %d\n", err);
		return false;
	}
	err = rmt_tx_wait_all_done(channel, portMAX_DELAY);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to transmit RMT data: %x\n", err);
		return false;
	}

	// Ensures RGB LED matrix is not updated until everything has latched
	ets_delay_us(LED_RMT_RESET_US);

	return true;
}

// Initializes tally RGB LED matrix
bool ws2812_init(void) {
	esp_err_t err;

	// Creates RMT channel
	rmt_tx_channel_config_t channel_config = {
		.gpio_num = PIN_TALLY_RGB,
		.resolution_hz = LED_RMT_FREQUENCY,
		.clk_src = RMT_CLK_SRC_DEFAULT,
		.mem_block_symbols = 64,
		.trans_queue_depth = 1
	};
	err = rmt_new_tx_channel(&channel_config, &channel);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to create RMT channel: %x\n", err);
		return false;
	}

	// Creates RMT encoder
	rmt_bytes_encoder_config_t encoder_config = {
		.bit0 = {
			.level0 = 1,
			.duration0 = LED_RMT_T0H_NS / (1000000000 / LED_RMT_FREQUENCY),
			.level1 = 0,
			.duration1 = LED_RMT_T0L_NS / (1000000000 / LED_RMT_FREQUENCY)
		},
		.bit1 = {
			.level0 = 1,
			.duration0 = LED_RMT_T1H_NS / (1000000000 / LED_RMT_FREQUENCY),
			.level1 = 0,
			.duration1 = LED_RMT_T1L_NS / (1000000000 / LED_RMT_FREQUENCY)
		},
		.flags.msb_first = 1
	};
	err = rmt_new_bytes_encoder(&encoder_config, &encoder);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to create RMT encoder: %x\n", err);
		return false;
	}

	// Enables RMT channel
	err = rmt_enable(channel);
	if (err != ESP_OK) {
		DEBUG_ERR_PRINTF("Failed to enable RMT: %x\n", err);
		return false;
	}

	// Turns off RGB tally LEDs for initial state
	ws2812_update(false, false);

	return true;
}

#endif // PIN_TALLY_RGB
