/*
 * gpio_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Low-level AVR GPIO initialization
 *
 * This file contains the earliest hardware bring-up code for
 * actuator, relay, and indicator GPIO pins on the ATmega1284P.
 *
 * DESIGN INTENT:
 *  - Configure all motor, solenoid, relay, and LED GPIO as outputs
 *  - Force a known-safe OFF state before any higher-level code runs
 *
 * SAFETY CRITICAL:
 *  - Several pins controlled here can energize motors or solenoids
 *  - This function MUST run before any scheduler, state machine,
 *    or interrupt is enabled
 *
 * MCU:
 *  - ATmega1284P
 *  - ISP only, no JTAG
 */

#include <avr/io.h>
#include "gpio_avr.h"

/* --------------------------------------------------------------------------
 * coop_gpio_init()
 *
 * Initializes all GPIO related to motors, solenoids, relays, and LEDs.
 *
 * SAFE DEFAULTS:
 *  - All H-bridge ENABLE pins are LOW (drivers disabled)
 *  - All INA/INB pins are LOW (no direction asserted)
 *  - Status LEDs OFF
 *
 * THIS FUNCTION MUST BE CALLED:
 *  - At the very start of main()
 *  - Before enabling interrupts
 * -------------------------------------------------------------------------- */
void coop_gpio_init(void)
{
    /* ------------------------------------------------------------------
     * Door + Lock H-bridge control (PORTA)
     * ------------------------------------------------------------------ */

    /* Configure outputs */
    DDRA |= (1u << DOOR_INA_BIT) |
            (1u << DOOR_INB_BIT) |
            (1u << DOOR_EN_BIT)  |
            (1u << LOCK_INA_BIT) |
            (1u << LOCK_INB_BIT) |
            (1u << LOCK_EN_BIT)  |
            (1u << LED_IN1_BIT)  |
            (1u << LED_IN2_BIT);

    /* Force known-safe OFF state */
    PORTA &= ~((1u << DOOR_INA_BIT) |
               (1u << DOOR_INB_BIT) |
               (1u << DOOR_EN_BIT)  |
               (1u << LOCK_INA_BIT) |
               (1u << LOCK_INB_BIT) |
               (1u << LOCK_EN_BIT)  |
               (1u << LED_IN1_BIT)  |
               (1u << LED_IN2_BIT));
}
