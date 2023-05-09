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

// Bit mask for both PGM and PVW pins
#define TALLY_MASK (PIN_PGM_MASK | PIN_PVW_MASK)

// Bit mask for CONN (connection) pin
#ifdef PIN_CONN
#define PIN_CONN_MASK (1 << PIN_CONN)
#else // PIN_CONN
#define PIN_CONN_MASK (0)
#endif // PIN_CONN



#ifdef ESP8266

#include <stdint.h> // uint32_t

#include <eagle_soc.h> // GPIO_PIN_COUNT, GPIO_REG_WRITE, GPIO_PIN0_ADDRESS, GPIO_ENABLE_W1TS_ADDRESS, RTC_GPIO_ENABLE, GPIO_OUT_W1TC_DATA_MASK, GPIO_OUT_W1TS_ADDRESS, GPIO_OUT_W1TC_ADDRESS, WRITE_PERI_REG, RTC_GPIO_OUT

// GPIO16 is not a normal GPIO pin and is processed in other registers as RTC pin
#define RTC_PIN 16

// Initializes GPIO or RTC pin
inline void led_init(uint32_t pin) {
	if (pin < GPIO_PIN_COUNT) {
		GPIO_REG_WRITE(GPIO_PIN0_ADDRESS + pin * 4, 0);
		GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, 1 << pin);
	}
	else if (pin == RTC_PIN) {
		WRITE_PERI_REG(RTC_GPIO_OUT, 0);
		WRITE_PERI_REG(RTC_GPIO_ENABLE, 1);
	}
}
#define LED_INIT(pin) led_init(pin)

// Separates RTC pin from normal GPIO pins
#define GPIO_MASK(mask) ((mask) & GPIO_OUT_W1TC_DATA_MASK)
#define RTC_MASK(mask) (((mask) >> RTC_PIN) & 1)

// Writes bit field to GPIO and/or RTC register
// Pin masks are able to be eliminated by compiler when defined deterministically
inline void gpio_write(uint32_t setPins, uint32_t setValue, uint32_t clearPins, uint32_t clearValue) {
	if (GPIO_MASK(setPins)) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, GPIO_MASK(setValue));\
	if (GPIO_MASK(clearPins)) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, GPIO_MASK(clearValue));\
	if (RTC_MASK(setPins | clearPins)) WRITE_PERI_REG(RTC_GPIO_OUT, RTC_MASK(setValue));\
}

// Writes CONN (connection) state as bit fields to register
#define LED_CONN_WRITE(setMask, clearMask) gpio_write(setMask, setMask, clearMask, clearMask)

// Writes tally states as bit fields to registers
#define LED_TALLY_WRITE(setMask, clearMask) gpio_write(TALLY_MASK, setMask, TALLY_MASK, clearMask)

#else // ESP8266

// Throws compilation error if any LED pin is used for platform without LED support
#if defined(PIN_PGM) || defined(PIN_PVW) || defined(PIN_CONN)
#error Platform does not support LED control
#endif // PIN_PGM || PIN_PVW || PIN_CONN

// Strips out LED setters for platforms without LED support
#define LED_TALLY(pgm, pvw)
#define LED_CONN(state)

#endif



// Gets tally bit masks from PGM and PVW states
#define TALLY_SET_MASK(pgm, pvw) ((pgm * PIN_PGM_MASK) | (pvw * PIN_PVW_MASK))
#define TALLY_CLR_MASK(pgm, pvw) ((!pgm * PIN_PGM_MASK) | (!pvw * PIN_PVW_MASK))

// Uses LED_TALLY_WRITE if LED_TALLY is not defined to convert states to bit fields
#ifndef LED_TALLY
#define LED_TALLY(pgm, pvw) LED_TALLY_WRITE(TALLY_SET_MASK(pgm, pvw), TALLY_CLR_MASK(pgm, pvw))
#endif // LED_TALLY

// Uses LED_CONN_WRITE if LED_CONN is not defined to convert state to bit field
#ifndef LED_CONN
#define LED_CONN(state) LED_CONN_WRITE(!state * PIN_CONN_MASK, state * PIN_CONN_MASK)
#endif // LED_CONN

#endif // LED_H
