// Include guard
#ifndef LED_H
#define LED_H

#include <stdbool.h> // bool, true
#include <stdint.h> // uint32_t

#include "./user_config.h" // PIN_CONN, PIN_PGM, PIN_PVW



// Bit mask for the PGM pin
#ifdef PIN_PGM
#define PIN_PGM_MASK (1 << PIN_PGM)
#else // PIN_PGM
#define PIN_PGM_MASK (0)
#endif // PIN_PGM

// Bit mask for the PVW pin
#ifdef PIN_PVW
#define PIN_PVW_MASK (1 << PIN_PVW)
#else // PIN_PVW
#define PIN_PVW_MASK (0)
#endif // PIN_PVW

// Bit mask for CONN (connection) pin
#ifdef PIN_CONN
#define PIN_CONN_MASK (1 << PIN_CONN)
#else // PIN_CONN
#define PIN_CONN_MASK (0)
#endif // PIN_CONN

// Bit mask for both tally pins
#define PIN_TALLY_MASK (PIN_PGM_MASK | PIN_PVW_MASK)



#ifdef ESP8266 /* ESP8266 NONOS SDK */
#include <eagle_soc.h> // GPIO_PIN_COUNT, GPIO_REG_WRITE, GPIO_PIN0_ADDRESS, GPIO_ENABLE_W1TS_ADDRESS, RTC_GPIO_ENABLE, GPIO_OUT_W1TC_DATA_MASK, GPIO_OUT_W1TS_ADDRESS, GPIO_OUT_W1TC_ADDRESS, WRITE_PERI_REG, RTC_GPIO_OUT

// GPIO16 is not a normal GPIO pin and is processed in other registers as RTC pin
#define RTC_PIN 16

// Initializes GPIO or RTC pin
static inline void led_init(const uint32_t pin) {
	if (pin < GPIO_PIN_COUNT) {
		GPIO_REG_WRITE(GPIO_PIN0_ADDRESS + pin * 4, 0);
		GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, 1 << pin);
	}
	else if (pin == RTC_PIN) {
		WRITE_PERI_REG(RTC_GPIO_OUT, 0);
		WRITE_PERI_REG(RTC_GPIO_ENABLE, 1);
	}
}

// Writes bit field to GPIO and/or RTC register
// Pin masks are known at compiletime so conditionals can be eliminated by compiler
static inline void gpio_write(const bool set, const bool clr, const uint32_t mask, const uint32_t values) {
	if (set && (mask & GPIO_OUT_W1TC_DATA_MASK)) {
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, values & GPIO_OUT_W1TC_DATA_MASK);
	}
	if (clr && (mask & GPIO_OUT_W1TC_DATA_MASK)) {
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (values ^ mask) & GPIO_OUT_W1TC_DATA_MASK);
	}
	if (mask & (1 << RTC_PIN)) {
		WRITE_PERI_REG(RTC_GPIO_OUT, (values >> RTC_PIN) & 1);
	}
}

#elif defined(ESP_PLATFORM) /* ESP-IDF */
#include <soc/gpio_struct.h> // GPIO
#include <hal/gpio_types.h> // gpio_num_t, GPIO_MODE_OUTPUT
#include <driver/gpio.h> // gpio_set_direction

// Initializes GPIO pin
static inline void led_init(const uint32_t pin) {
	gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
}

// Writes bit field to GPIO register
// Pin masks are known at compiletime so conditionals can be eliminated by compiler
static inline void gpio_write(const bool set, const bool clr, const uint32_t mask, const uint32_t values) {
	if (set) {
		GPIO.out_w1ts.val = values;
	}
	if (clr) {
		GPIO.out_w1tc.val = values ^ mask;
	}
}

#else

// Throws compilation error if any LED pin is used for platform without LED support
#if defined(PIN_PGM) || defined(PIN_PVW) || defined(PIN_CONN)
#error Platform does not support LED control
#endif // PIN_PGM || PIN_PVW || PIN_CONN

// Strips out LED setters for platforms without LED support
#define LED_TALLY(pgm, pvw)
#define LED_CONN(state)

#endif



// Uses gpio_write if LED_TALLY is not defined to write tally states to registers as bit fields
#ifndef LED_TALLY
#define LED_TALLY(pgm, pvw) gpio_write(true, true, PIN_TALLY_MASK, ((pgm) * PIN_PGM_MASK) | ((pvw) * PIN_PVW_MASK))
#endif // LED_TALLY

// Uses gpio_write if LED_CONN is not defined to write connection state to register as bit field
#ifndef LED_CONN
#define LED_CONN(state) gpio_write((state), (!state), PIN_CONN_MASK, (state) * PIN_CONN_MASK)
#endif // LED_CONN

#endif // LED_H
