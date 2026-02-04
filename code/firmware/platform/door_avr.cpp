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
 *  - Masked PORTA writes only
 *
 * Updated: 2026-02-03
 */

#include "door_hw.h"
#include "gpio_avr.h"

#include <avr/io.h>
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Internal helpers (masked writes only)
 * -------------------------------------------------------------------------- */

static inline void set_bits(uint8_t mask)
{
    PORTA |= mask;
}

static inline void clear_bits(uint8_t mask)
{
    PORTA &= (uint8_t)~mask;
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
    DDRA |= (1u << DOOR_INA_BIT) |
            (1u << DOOR_INB_BIT) |
            (1u << DOOR_EN_BIT);

    /* Safe default:
     *  - EN  = 0 (motor off)
     *  - INA = 0
     *  - INB = 0
     */
    clear_bits((1u << DOOR_EN_BIT) |
               (1u << DOOR_INA_BIT) |
               (1u << DOOR_INB_BIT));

    init = 1;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void door_hw_set_open_dir(void)
{
    door_hw_init_once();

    /* INA = 1, INB = 0 */
    clear_bits(1u << DOOR_INB_BIT);
    set_bits(1u << DOOR_INA_BIT);
}

void door_hw_set_close_dir(void)
{
    door_hw_init_once();

    /* INA = 0, INB = 1 */
    clear_bits(1u << DOOR_INA_BIT);
    set_bits(1u << DOOR_INB_BIT);
}

void door_hw_enable(void)
{
    door_hw_init_once();
    set_bits(1u << DOOR_EN_BIT);
}

void door_hw_disable(void)
{
    door_hw_init_once();
    clear_bits(1u << DOOR_EN_BIT);
}

void door_hw_stop(void)
{
    door_hw_init_once();

    /* Disable power first, then neutralize direction */
    clear_bits((1u << DOOR_EN_BIT) |
               (1u << DOOR_INA_BIT) |
               (1u << DOOR_INB_BIT));
}
