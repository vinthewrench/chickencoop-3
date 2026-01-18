/*
 * relay_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: AVR relay driver for DSP1-L2-DC12V latching relays
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *  - Dual-coil latching relays (SET / RESET)
 *  - Driven via ULN2003
 *
 * Hardware mapping (ATmega32U4):
 *   RELAY_1_SET   -> PD4 (pin 25)
 *   RELAY_1_RESET -> PD5 (pin 22)
 *   RELAY_2_SET   -> PD6 (pin 26)
 *   RELAY_2_RESET -> PD7 (pin 27)
 *
 * Electrical rules:
 *  - Only one relay coil may be energized at a time
 *  - Pulse-driven, no holding current
 *  - Pulse width ~20 ms (datasheet max operate/reset ~10 ms)
 *
 * Safety rules:
 *  - Masked bit operations ONLY
 *  - Never write whole PORTD or DDRD
 *  - Never touch PD0/PD1 (I2C)
 *
 * Updated: 2025-12-29
 */

#include "relay_hw.h"

#include <avr/io.h>
#include <util/delay.h>

/* --------------------------------------------------------------------------
 * Configuration
 * -------------------------------------------------------------------------- */

#include "gpio_avr.h"

#define RELAY_PULSE_MS  20

#define RELAY_ALL_BITS \
    ((1 << RELAY1_SET_BIT)   | \
     (1 << RELAY1_RESET_BIT) | \
     (1 << RELAY2_SET_BIT)   | \
     (1 << RELAY2_RESET_BIT))

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

/*
 * Initialize relay GPIO.
 * Must be called exactly once at startup.
 */
void relay_init(void)
{
    /* Configure relay pins as outputs (masked, no side effects) */
    DDRD |= RELAY_ALL_BITS;

    /* Ensure all relay outputs are de-energized */
    PORTD &= ~RELAY_ALL_BITS;
}

/*
 * Pulse a single relay coil.
 * Assumes relay_init() has already been called.
 */
static inline void relay_pulse(uint8_t bit)
{
    /* Enforce mutual exclusion: all relay coils OFF */
    PORTD &= ~RELAY_ALL_BITS;

    /* Energize selected coil */
    PORTD |= (1 << bit);

    /* Pulse width per relay datasheet */
    _delay_ms(RELAY_PULSE_MS);

    /* De-energize coil */
    PORTD &= ~(1 << bit);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void relay1_set(void)
{
    relay_pulse(RELAY1_SET_BIT);
}

void relay1_reset(void)
{
    relay_pulse(RELAY1_RESET_BIT);
}

void relay2_set(void)
{
    relay_pulse(RELAY2_SET_BIT);
}

void relay2_reset(void)
{
    relay_pulse(RELAY2_RESET_BIT);
}
