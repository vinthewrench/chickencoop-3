/*
 * lock_hw.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator hardware interface
 *
 * Notes:
 *  - Pure hardware abstraction
 *  - Direction via INA/INB, power via EN
 *  - No timing, no state, no policy
 *
 * LOCKED DESIGN – do not extend this layer
 *
 * Updated: 2026-01-02
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void lock_hw_init(void);

/* Set direction only (does not apply power) */
void lock_hw_set_lock_dir(void);     /* engage */
void lock_hw_set_unlock_dir(void);   /* disengage */

/* Power gate */
void lock_hw_enable(void);
void lock_hw_disable(void);

/* Safe stop: EN=0, INA=0, INB=0 */
void lock_hw_stop(void);

#ifdef __cplusplus
}
#endif
