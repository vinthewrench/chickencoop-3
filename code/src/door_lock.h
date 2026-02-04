#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * door_lock.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Door lock actuator driver
 *
 * DESIGN INTENT
 * -------------
 * This module directly drives a door lock actuator (automotive-style
 * lock motor / solenoid) via an H-bridge.
 *
 * Key properties:
 *  - BLOCKING by design
 *  - Enforced maximum on-time (hardware safety)
 *  - No background state
 *  - No dependence on main loop timing
 *
 * SAFETY CONTRACT
 * ---------------
 * If any function in this module returns, the lock output is OFF.
 * There is no scenario where the actuator can remain powered
 * due to scheduler failure, missed ticks, or logic bugs upstream.
 *
 * The caller is allowed to stall while the lock is energized.
 * This is intentional and required for safety.
 */

/* Initialize lock GPIO and force safe OFF state (idempotent) */
void door_lock_init(void);

/*
 * Engage the lock (blocking pulse).
 * Direction is fixed and enforced internally.
 */
void door_lock_engage(void);

/*
 * Release the lock (blocking pulse).
 * Direction is fixed and enforced internally.
 */
void door_lock_release(void);

/*
 * Immediately disable lock output.
 * Safe to call at any time.
 */
void door_lock_stop(void);

#ifdef __cplusplus
}
#endif
