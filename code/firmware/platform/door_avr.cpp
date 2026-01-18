/*
 * door_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door actuator hardware control (AVR) using VNH7100BASTR
 *
 * Notes:
 *  - Pure hardware driver
 *  - Direction via INA / INB
 *  - Power gated via EN (used as digital enable, no PWM)
 *  - No timing, no state, no policy
 *  - Masked PORTF writes only
 *
 * Updated: 2026-01-05
 */

#include "door_hw.h"

#include <avr/io.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Pin mapping (LOCKED to board schematic)
 * -------------------------------------------------------------------------- */
/* DOOR_INA -> PF5 (pin 38) */
/* DOOR_INB -> PF6 (pin 37) */
/* DOOR_EN  -> PF7 (pin 36) */

#include "gpio_avr.h"

/* --------------------------------------------------------------------------
 * Internal helpers (masked writes only)
 * -------------------------------------------------------------------------- */

static inline void set_bits(uint8_t mask)
{
    PORTF |= mask;
}

static inline void clear_bits(uint8_t mask)
{
    PORTF &= (uint8_t)~mask;
}

/* --------------------------------------------------------------------------
 * One-time hardware init guard
 * -------------------------------------------------------------------------- */

static inline void door_hw_init_once(void)
{
    static uint8_t init = 0;
    if (init)
        return;

    /* Configure control pins as outputs */
    DDRF |= (DOOR_INA_BIT | DOOR_INB_BIT | DOOR_EN_BIT);

    /* Safe default:
     *  - EN = 0  (motor off)
     *  - INA = 0
     *  - INB = 0
     */
    clear_bits(DOOR_EN_BIT | DOOR_INA_BIT | DOOR_INB_BIT);

    init = 1;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_hw_set_open_dir(void)
{
    door_hw_init_once();

    /* INA=1, INB=0 */
    clear_bits(DOOR_INB_BIT);
    set_bits(DOOR_INA_BIT);
}

void door_hw_set_close_dir(void)
{
    door_hw_init_once();

    /* INA=0, INB=1 */
    clear_bits(DOOR_INA_BIT);
    set_bits(DOOR_INB_BIT);
}

void door_hw_enable(void)
{
    door_hw_init_once();
    set_bits(DOOR_EN_BIT);
}

void door_hw_disable(void)
{
    door_hw_init_once();
    clear_bits(DOOR_EN_BIT);
}

void door_hw_stop(void)
{
    door_hw_init_once();

    /* Disable power first, then neutralize direction */
    clear_bits(DOOR_EN_BIT);
    clear_bits(DOOR_INA_BIT | DOOR_INB_BIT);
}
