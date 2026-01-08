/*
 * door_state_machine.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Door motion state machine (internal)
 *
 * Responsibilities:
 *  - Serialize door open/close requests
 *  - Enforce time-based motion (no sensors)
 *  - Coordinate lock sequencing safely
 *  - Abort-and-restart on new command
 *
 * Invariants:
 *  - Door ALWAYS unlocks before motion
 *  - Door NEVER moves while locked
 *  - Lock engages ONLY after close completes
 *  - OPEN is the safe default
 *
 * Notes:
 *  - Non-blocking, tick-driven state machine
 *  - dev_state_t expresses external intent only
 *  - Internal motion states represent physical truth
 */

#pragma once

#include <stdint.h>
#include "device.h"   /* dev_state_t */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Internal door motion states (private truth)
 * -------------------------------------------------------------------------- */
typedef enum {
    /* Unknown at boot */
    DOOR_IDLE_UNKNOWN = 0,

    /* Settled states */
    DOOR_IDLE_OPEN,
    DOOR_IDLE_CLOSED,

    /* Transitional states */
    DOOR_PREOPEN_UNLOCK,     /* waiting for unlock before opening */
    DOOR_MOVING_OPEN,

    DOOR_PRECLOSE_UNLOCK,    /* waiting for unlock before closing */
    DOOR_MOVING_CLOSE,

    DOOR_POSTCLOSE_LOCK      /* engaging lock after close */
} door_motion_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/*
 * Initialize door state machine.
 *
 * - Called once at boot
 * - Does not move hardware
 * - Initial state is UNKNOWN until commanded
 */
void door_sm_init(void);

/*
 * Request a new door state.
 *
 * Parameters:
 *  - state:
 *      DEV_STATE_ON  → OPEN
 *      DEV_STATE_OFF → CLOSE
 *
 * Behavior:
 *  - Edge-triggered
 *  - Mid-motion requests abort current action safely
 *  - Repeated requests for same settled state are ignored
 */
void door_sm_request(dev_state_t state);

/*
 * Periodic service function.
 *
 * Parameters:
 *  - now_ms: current system time in milliseconds
 *
 * Must be called regularly to advance sequencing and timing.
 */
void door_sm_tick(uint32_t now_ms);

/*
 * Query the settled, device-visible door state.
 *
 * Returns:
 *  - DEV_STATE_ON      → door open
 *  - DEV_STATE_OFF     → door closed
 *  - DEV_STATE_UNKNOWN → moving, locking, or unknown
 */
dev_state_t door_sm_get_state(void);

/*
 * Query internal motion state.
 *
 * Intended for:
 *  - LED behavior
 *  - Diagnostics / debug
 *
 * Returns:
 *  - door_motion_t (internal physical truth)
 */
door_motion_t door_sm_get_motion(void);

#ifdef __cplusplus
}
#endif
