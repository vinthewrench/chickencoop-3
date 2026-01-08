#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
 * lock_hw.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator hardware abstraction
 *
 * Notes:
 *  - Hardware-only interface
 *  - No timing, no state machine
 *  - Implemented per platform (AVR, host)
 */

/* Initialize lock hardware (idempotent) */
void lock_hw_init(void);

/* Drive lock actuator in ENGAGE direction */
void lock_hw_engage(void);

/* Drive lock actuator in RELEASE direction */
void lock_hw_release(void);

/* Immediately disable lock output (safe stop) */
void lock_hw_stop(void);

#ifdef __cplusplus
}
#endif
