#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "device.h"   /* dev_state_t */


#ifdef __cplusplus
extern "C" {
#endif

/*
 * lock_state_machine.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Lock actuator state machine
 *
 * Notes:
 *  - Platform-agnostic
 *  - Enforces safe solenoid pulse timing
 *  - Door logic requests intent only
 *  - Hardware access via lock_hw_*
 */

/* Initialize lock state machine */
void lock_sm_init(void);

/* Requests (ignored if lock is busy) */
void lock_sm_engage(void);
void lock_sm_release(void);

/* Periodic service (must be called regularly) */
void lock_sm_tick(uint32_t now_ms);

/* Queries */
bool lock_sm_busy(void);
bool lock_sm_is_engaging(void);
bool lock_sm_is_releasing(void);


/*
 * Query the settled, device-visible lock state.
 *
 * Returns:
 *  - DEV_STATE_ON      → lock engaged
 *  - DEV_STATE_OFF     → lock released
 *  - DEV_STATE_UNKNOWN → in motion or unknown
 */
dev_state_t lock_sm_get_state(void);

#ifdef __cplusplus
}
#endif
