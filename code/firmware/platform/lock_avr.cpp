/*
 * lock_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator hardware control (AVR)
 *
 * Notes:
 *  - Pure hardware driver
 *  - Direction via INA / INB
 *  - Power gated via EN
 *  - No timing, no state, no policy
 *  - Masked PORTF writes only
 *
 * Updated: 2026-01-02
 */

#include "lock_hw.h"

#include <avr/io.h>

/* --------------------------------------------------------------------------
 * Pin mapping (LOCKED to board schematic)
 * -------------------------------------------------------------------------- */
/* LOCK_INA -> PF0 */
/* LOCK_INB -> PF1 */
/* LOCK_EN  -> PF4 */

#define LOCK_INA_BIT   (1u << PF0)
#define LOCK_INB_BIT   (1u << PF1)
#define LOCK_EN_BIT    (1u << PF4)

/* --------------------------------------------------------------------------
 * Internal helpers (masked writes)
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
 * Public API
 * -------------------------------------------------------------------------- */

void lock_hw_init(void)
{
    /* Configure control pins as outputs */
    DDRF |= (LOCK_INA_BIT | LOCK_INB_BIT | LOCK_EN_BIT);

    /* Safe default:
     *  - EN = 0  (motor off)
     *  - INA = 0
     *  - INB = 0
     */
    clear_bits(LOCK_EN_BIT | LOCK_INA_BIT | LOCK_INB_BIT);
}

void lock_hw_set_lock_dir(void)
{
    /* INA=1, INB=0 */
    clear_bits(LOCK_INB_BIT);
    set_bits(LOCK_INA_BIT);
}

void lock_hw_set_unlock_dir(void)
{
    /* INA=0, INB=1 */
    clear_bits(LOCK_INA_BIT);
    set_bits(LOCK_INB_BIT);
}

void lock_hw_enable(void)
{
    set_bits(LOCK_EN_BIT);
}

void lock_hw_disable(void)
{
    clear_bits(LOCK_EN_BIT);
}

void lock_hw_stop(void)
{
    /* Disable power first, then neutralize direction */
    clear_bits(LOCK_EN_BIT);
    clear_bits(LOCK_INA_BIT | LOCK_INB_BIT);
}
