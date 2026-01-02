/*
 * relay.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Relay control interface
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
 * Initialization
 * -------------------------------------------------------------------------- */

/*
 * Initialize relay GPIO.
 * Must be called once at startup before any relay_* calls.
 */
void relay_init(void);

/* --------------------------------------------------------------------------
 * Relay control API
 * -------------------------------------------------------------------------- */

void relay1_set(void);
void relay1_reset(void);

void relay2_set(void);
void relay2_reset(void);

#ifdef __cplusplus
}
#endif
