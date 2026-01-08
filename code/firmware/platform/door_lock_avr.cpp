/*
 * lock_hw_avr.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator hardware driver (AVR)
 *
 * Responsibilities:
 *  - Drive lock actuator via H-bridge
 *  - Provide immediate engage / release / stop
 *
 * Notes:
 *  - NO timing logic
 *  - NO safety enforcement
 *  - NO state machine
 *  - All sequencing handled by lock_state_machine
 *
 * Hardware (LOCKED, V3.0):
 *  - VNH7100BASTR H-bridge
 *  - LOCK_INA -> PF0
 *  - LOCK_INB -> PF1
 *  - LOCK_EN  -> PF4
 *
 * Updated: 2026-01-07
 */

#include <avr/io.h>
#include <stdint.h>

#include "lock_hw.h"

/* --------------------------------------------------------------------------
 * Pin mapping
 * -------------------------------------------------------------------------- */

#define LOCK_INA_BIT   (1u << PF0)
#define LOCK_INB_BIT   (1u << PF1)
#define LOCK_EN_BIT    (1u << PF4)

/* --------------------------------------------------------------------------
 * Helpers (masked writes only)
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
 * Public hardware API
 * -------------------------------------------------------------------------- */

void lock_hw_init(void)
{
    /* Configure control pins as outputs */
    DDRF |= (LOCK_INA_BIT | LOCK_INB_BIT | LOCK_EN_BIT);

    /* Safe default */
    lock_hw_stop();
}

void lock_hw_stop(void)
{
    /* Disable power first, then neutralize direction */
    clear_bits(LOCK_EN_BIT);
    clear_bits(LOCK_INA_BIT | LOCK_INB_BIT);
}

void lock_hw_engage(void)
{
    /* INA=1, INB=0 */
    clear_bits(LOCK_INB_BIT);
    set_bits(LOCK_INA_BIT);

    /* EN=1 */
    set_bits(LOCK_EN_BIT);
}

void lock_hw_release(void)
{
    /* INA=0, INB=1 */
    clear_bits(LOCK_INA_BIT);
    set_bits(LOCK_INB_BIT);

    /* EN=1 */
    set_bits(LOCK_EN_BIT);
}
