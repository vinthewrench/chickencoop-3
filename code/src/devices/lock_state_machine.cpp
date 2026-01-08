/*
 * lock_state_machine.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator state machine (platform-agnostic)
 *
 * Responsibilities:
 *  - Enforce safe solenoid pulse timing
 *  - Serialize engage/release requests
 *  - Prevent re-entry and overdrive
 *
 * Notes:
 *  - Contains NO hardware register access
 *  - All physical I/O performed via lock_hw_*
 *  - Must be serviced periodically via lock_sm_tick()
 *
 * Hardware policy:
 *  - Lock pulse duration is fixed and safety-critical
 *  - Door logic requests intent only
 *
 * Updated: 2026-01-07
 */

#include "lock_state_machine.h"
#include "lock_hw.h"
#include "device.h"   /* dev_state_t */

/* --------------------------------------------------------------------------
 * Configuration (LOCKED)
 * -------------------------------------------------------------------------- */

#define LOCK_PULSE_MS 500u

/* --------------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------------- */

typedef enum {
    LOCK_STATE_IDLE = 0,
    LOCK_STATE_ENGAGING,
    LOCK_STATE_RELEASING
} lock_state_t;

static lock_state_t g_state = LOCK_STATE_IDLE;
static uint32_t     g_t0_ms = 0;

/*
 * Last known mechanical truth:
 *  - DEV_STATE_ON  → locked
 *  - DEV_STATE_OFF → unlocked
 *  - DEV_STATE_UNKNOWN → never commanded since boot
 */
static dev_state_t  g_settled_state = DEV_STATE_UNKNOWN;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void lock_sm_init(void)
{
    lock_hw_init();
    lock_hw_stop();

    g_state = LOCK_STATE_IDLE;
    g_t0_ms = 0;
    g_settled_state = DEV_STATE_UNKNOWN;
}

void lock_sm_engage(void)
{
    if (g_state != LOCK_STATE_IDLE)
        return;

    lock_hw_engage();
    g_state = LOCK_STATE_ENGAGING;
    g_t0_ms = 0;
}

void lock_sm_release(void)
{
    if (g_state != LOCK_STATE_IDLE)
        return;

    lock_hw_release();
    g_state = LOCK_STATE_RELEASING;
    g_t0_ms = 0;
}

void lock_sm_tick(uint32_t now_ms)
{
    if (g_state == LOCK_STATE_IDLE)
        return;

    /* Arm start time on first tick */
    if (g_t0_ms == 0) {
        g_t0_ms = now_ms;
        return;
    }

    if ((uint32_t)(now_ms - g_t0_ms) >= LOCK_PULSE_MS) {
        lock_hw_stop();

        /* Update mechanical truth only when pulse completes */
        if (g_state == LOCK_STATE_ENGAGING)
            g_settled_state = DEV_STATE_ON;   /* locked */
        else if (g_state == LOCK_STATE_RELEASING)
            g_settled_state = DEV_STATE_OFF;  /* unlocked */

        g_state = LOCK_STATE_IDLE;
        g_t0_ms = 0;
    }
}

/* --------------------------------------------------------------------------
 * Queries
 * -------------------------------------------------------------------------- */

bool lock_sm_busy(void)
{
    return g_state != LOCK_STATE_IDLE;
}

bool lock_sm_is_engaging(void)
{
    return g_state == LOCK_STATE_ENGAGING;
}

bool lock_sm_is_releasing(void)
{
    return g_state == LOCK_STATE_RELEASING;
}

/*
 * Query mechanical lock state.
 *
 * Returns last known physical truth, even during pulses.
 */
dev_state_t lock_sm_get_state(void)
{
    return g_settled_state;
}
