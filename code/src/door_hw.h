/*
 * door_hw.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Door actuator hardware interface (VNH7100BASTR)
 *
 * Notes:
 *  - Pure hardware abstraction
 *  - Direction via INA / INB
 *  - Power gated via EN (PWM pin used as digital enable)
 *  - No timing, no state, no policy
 *
 * LOCKED DESIGN
 *
 * Updated: 2026-01-02
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize door actuator control pins */
void door_hw_init(void);

/* Set direction only (does not apply power) */
void door_hw_set_open_dir(void);    /* extend */
void door_hw_set_close_dir(void);   /* retract */

/* Power gate */
void door_hw_enable(void);
void door_hw_disable(void);

/* Safe stop: EN=0, INA=0, INB=0 */
void door_hw_stop(void);

#ifdef __cplusplus
}
#endif
