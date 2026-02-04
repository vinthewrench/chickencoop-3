/*
 * door_lock_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door lock actuator driver (AVR implementation)
 *
 * HARDWARE
 * --------
 *  - Automotive-style lock motor / solenoid
 *  - Driven through an H-bridge (VNH7100 or equivalent)
 *
 * PIN MAPPING (LOCKED BY HARDWARE DESIGN)
 * --------------------------------------
 *  - LOCK_INA -> Direction A
 *  - LOCK_INB -> Direction B
 *  - LOCK_EN  -> Power enable
 *
 * DESIGN INTENT
 * -------------
 * This module is intentionally SIMPLE and DEFENSIVE.
 *
 *  - Blocking operation is REQUIRED and intentional
 *  - No timers, no interrupts, no background state
 *  - No dependency on scheduler cadence or main loop health
 *
 * SAFETY GUARANTEES
 * -----------------
 *  - A hard maximum on-time is always enforced
 *  - Power is ALWAYS cut before this code returns
 *  - Direction is never changed while power is enabled
 *
 * FAILURE MODEL
 * -------------
 * If this code misbehaves, it FAILS SAFE:
 *  - EN is cleared
 *  - Direction lines are neutralized
 *  - Motor is de-energized
 *
 * RATIONALE
 * ---------
 * Door locks are high-current, thermally fragile devices.
 * This code exists specifically to prevent:
 *  - Motor burnout
 *  - H-bridge thermal failure
 *  - Board damage from software hangs
 *
 * Updated: 2026-02-03
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#include "door_lock.h"
#include "gpio_avr.h"
#include "config.h"

/*
 * HARD SAFETY LIMIT (milliseconds)
 *
 * Absolute maximum time the lock actuator may be energized,
 * regardless of configuration or EEPROM corruption.
 *
 * This value exists to prevent thermal damage or motor burnout
 * under ALL circumstances.
 */
#define LOCK_MAX_PULSE_MS  1500u

/* --------------------------------------------------------------------------
 * Low-level helpers (masked writes only)
 * -------------------------------------------------------------------------- */

/* Set one or more PORTA bits atomically */
static inline void set_bits(uint8_t mask)
{
    PORTA |= mask;
}

/* Clear one or more PORTA bits atomically */
static inline void clear_bits(uint8_t mask)
{
    PORTA &= (uint8_t)~mask;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_lock_init(void)
{
    /*
     * Configure H-bridge control pins as outputs.
     * Pin mapping is fixed by hardware design.
     */
    DDRA |= (1u << LOCK_INA_BIT) |
            (1u << LOCK_INB_BIT) |
            (1u << LOCK_EN_BIT);

    /* Force a known-safe state */
    door_lock_stop();
}

/*
 * Internal helper: apply a blocking lock pulse in a given direction.
 *
 * Parameters:
 *  - ina: desired logic level for INA
 *  - inb: desired logic level for INB
 *
 * This function:
 *  - Forces a clean OFF baseline
 *  - Applies direction
 *  - Enables power
 *  - Enforces a hard maximum on-time
 *  - Guarantees power is OFF on exit
 */
static void lock_pulse(uint8_t ina, uint8_t inb)
{
    /*
     * Defensive baseline:
     * Ensure the H-bridge is fully disabled before changing direction.
     */
    door_lock_stop();

    /*
     * Small dead-time to allow bridge discharge and avoid
     * shoot-through on direction changes.
     */
    _delay_ms(5);

    /* Apply direction (INA / INB) */
    if (ina)
        set_bits(1u << LOCK_INA_BIT);
    else
        clear_bits(1u << LOCK_INA_BIT);

    if (inb)
        set_bits(1u << LOCK_INB_BIT);
    else
        clear_bits(1u << LOCK_INB_BIT);

    /*
     * Determine pulse length.
     * Configuration value is bounded by a hard safety cap.
     */
    uint16_t ms = g_cfg.lock_pulse_ms;
    if (ms == 0 || ms > LOCK_MAX_PULSE_MS)
        ms = LOCK_MAX_PULSE_MS;

    /*
     * Enable power only after direction is stable
     * and pulse duration is known.
     */
    set_bits(1u << LOCK_EN_BIT);

    /* Blocking delay: intentional and required for safety */
    while (ms--)
        _delay_ms(1);

    /* Always shut down power before returning */
    door_lock_stop();
}

void door_lock_engage(void)
{
    /*
     * Engage direction:
     * INA = 1, INB = 0
     */
    lock_pulse(1, 0);
}

void door_lock_release(void)
{
    /*
     * Release direction:
     * INA = 0, INB = 1
     */
    lock_pulse(0, 1);
}

void door_lock_stop(void)
{
    /*
     * Kill power FIRST.
     * This guarantees the motor is de-energized
     * before changing or clearing direction.
     */
    clear_bits(1u << LOCK_EN_BIT);

    /*
     * Then neutralize direction lines.
     * Leaves the bridge in a passive, safe state.
     */
    clear_bits((1u << LOCK_INA_BIT) |
               (1u << LOCK_INB_BIT));
}
