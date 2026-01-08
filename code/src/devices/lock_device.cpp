/*
 * lock_device.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Door device adapter (Device API → door state machine)
 *
 * Notes:
 *  - Implements Device interface
 *  - Delegates motion and timing to door_state_machine
 *  - No direct hardware control here
 *
 * Updated: 2026-01-06
 */

#include "device.h"
#include "lock_state_machine.h"

/*
 * Device-visible state only.
 * This reflects settled truth, not motion.
 */
static dev_state_t lock_get_state(void)
{
    return lock_sm_get_state();
}

static void lock_set_state(dev_state_t state)
{

    if (state == DEV_STATE_ON)
        lock_sm_engage();
    else if (state == DEV_STATE_OFF)
        lock_sm_release();
}

static const char *lock_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "LOCK";
    case DEV_STATE_OFF: return "UNLOCK";
    default:            return "UNKNOWN";
    }
}

static void lock_init(void)
{
    lock_sm_init();
}

static void lock_tick(uint32_t now_ms)
{
    lock_sm_tick(now_ms);
}

/* --------------------------------------------------------------------------
 * Device registration
 * -------------------------------------------------------------------------- */

Device lock_device = {
    .name         = "lock",
    .init         = lock_init,
    .get_state    = lock_get_state , //lock_get_state,
    .set_state    = lock_set_state,
    .state_string = lock_state_string,
    .tick         = lock_tick
};
