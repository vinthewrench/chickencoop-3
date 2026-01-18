/*
 * gpio_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Low-level AVR GPIO initialization and JTAG disable
 *
 * This file contains the earliest hardware bring-up code for
 * actuator and relay GPIO pins on the ATmega32U4.
 *
 * DESIGN INTENT:
 *  - Disable JTAG at runtime so PF4–PF7 are usable as GPIO.
 *  - Configure all motor, solenoid, and relay control pins as outputs.
 *  - Force a known-safe OFF state on all drivers before any higher-
 *    level code executes.
 *
 * SAFETY CRITICAL:
 *  - Several pins controlled here can energize motors or solenoids.
 *  - Any failure to initialize these pins correctly can cause
 *    unintended motion or hardware damage.
 *  - This code MUST run before schedulers, state machines, or
 *    interrupts are enabled.
 */

#include <avr/io.h>
#include "gpio_avr.h"


/* --------------------------------------------------------------------------
 * disable_jtag_runtime()
 *
 * PF4–PF7 are multiplexed with the JTAG interface when the JTAGEN
 * fuse is programmed (which it is on this project).
 *
 * Writing the JTD bit in MCUCR disables JTAG until the next reset.
 * The AVR datasheet requires this bit to be written twice within
 * four CPU cycles for the change to take effect.
 *
 * NOTES:
 *  - This does NOT affect ISP programming.
 *  - JTAG will be re-enabled automatically on reset.
 *  - This must be done before configuring DDRF or PORTF.
 * -------------------------------------------------------------------------- */
static inline void disable_jtag_runtime(void)
{
    MCUCR |= (1 << JTD);
    MCUCR |= (1 << JTD);   /* Must be written twice within 4 cycles */
}


/* --------------------------------------------------------------------------
 * coop_gpio_init()
 *
 * Initializes all GPIO related to motors, solenoids, and relays.
 *
 * Initialization sequence:
 *  1. Disable JTAG so PF4–PF7 become normal GPIO.
 *  2. Configure all actuator-related pins as outputs.
 *  3. Drive all outputs to a known-safe OFF state.
 *
 * SAFE DEFAULTS:
 *  - All H-bridge ENABLE pins are LOW (drivers disabled).
 *  - All INA/INB pins are LOW (no direction asserted).
 *
 * FAILURE MODES PREVENTED:
 *  - Accidental motor or solenoid activation at boot.
 *  - GPIO writes silently ignored due to JTAG ownership.
 *  - Actuators energized before firmware safety timers are active.
 *
 * THIS FUNCTION MUST BE CALLED:
 *  - At the very start of main()
 *  - Before enabling interrupts
 *  - Before starting any scheduler or state machine
 * -------------------------------------------------------------------------- */
void coop_gpio_init(void)
{
    /* Disable JTAG so PF4–PF7 become usable GPIO */
    disable_jtag_runtime();

    /* Configure door and lock control pins as outputs */
    DDRF |= DOOR_INA_BIT |
            DOOR_INB_BIT |
            DOOR_EN_BIT  |
            LOCK_INA_BIT |
            LOCK_INB_BIT |
            LOCK_EN_BIT;

    /* Force known-safe OFF state:
     *  - ENABLE lines low → H-bridges disabled
     *  - Direction lines low → no drive selected
     */
    PORTF &= ~(DOOR_INA_BIT |
               DOOR_INB_BIT |
               DOOR_EN_BIT  |
               LOCK_INA_BIT |
               LOCK_INB_BIT |
               LOCK_EN_BIT);

    /* ------------------------------------------------------------------
     * Configure door status LEDs (PORTB)
     * ------------------------------------------------------------------ */
    DDRB |= (1u << LED_IN1) |
            (1u << LED_IN2);

    /* LEDs OFF at boot */
    PORTB &= ~((1u << LED_IN1) |
               (1u << LED_IN2));

}
