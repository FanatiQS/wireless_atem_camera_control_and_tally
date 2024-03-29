// Include guard
#ifndef LED_H
#define LED_H

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



#ifdef ESP8266

#include <stdint.h> // uint32_t

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
static inline void gpio_write(const uint32_t setMask, const uint32_t clearMask, const uint32_t values) {
	if (setMask & GPIO_OUT_W1TC_DATA_MASK) {
		GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, values & GPIO_OUT_W1TC_DATA_MASK);
	}
	if (clearMask & GPIO_OUT_W1TC_DATA_MASK) {
		GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, ~values & clearMask & GPIO_OUT_W1TC_DATA_MASK);
	}
	if (((setMask | clearMask) >> RTC_PIN) & 1) {
		WRITE_PERI_REG(RTC_GPIO_OUT, (values >> RTC_PIN) & 1);
	}
}

#else // ESP8266

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
#define LED_TALLY(pgm, pvw) gpio_write(PIN_TALLY_MASK, PIN_TALLY_MASK, (pgm * PIN_PGM_MASK) | (pvw * PIN_PVW_MASK))
#endif // LED_TALLY

// Uses gpio_write if LED_CONN is not defined to write connection state to register as bit field
#ifndef LED_CONN
#define LED_CONN(state) gpio_write(!state * PIN_CONN_MASK, state * PIN_CONN_MASK, !state * PIN_CONN_MASK)
#endif // LED_CONN

#endif // LED_H
