/*
 * lock.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock stub (transitional)
 *
 * Notes:
 *  - TEMPORARY glue
 *  - No timing, no scheduling
 *  - State is assumed, not sensed
 *  - Will be replaced by door_control logic
 *
 * Updated: 2026-01-02
 */

#include "lock.h"
#include "lock_hw.h"

/*
 * This is NOT physical truth.
 * It only reflects the last requested intent.
 */
static bool g_lock_assumed_engaged = false;

void lock_engage(void)
{
    /* Engage lock */
    lock_hw_set_lock_dir();
    lock_hw_enable();

    g_lock_assumed_engaged = true;
}

void lock_release(void)
{
    /* Release lock */
    lock_hw_set_unlock_dir();
    lock_hw_enable();

    g_lock_assumed_engaged = false;
}

bool lock_is_engaged(void)
{
    /*
     * Conservative assumption based on last request.
     * Real implementation will query control state.
     */
    return g_lock_assumed_engaged;
}
