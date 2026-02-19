/*
 * devices.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Device registry public interface
 *
 * Design goals:
 *  - Static, sparse device table
 *  - Explicit device IDs (not positional)
 *  - No exposure of Device struct to callers
 *  - Enumeration instead of index probing
 *  - Safe defaults (UNKNOWN state when unsupported)
 *
 * Notes:
 *  - All functions are deterministic
 *  - No dynamic memory
 *  - Caller must not assume contiguous IDs
 *
 * Updated: 2026-01-08
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "device.h"   /* Device, dev_state_t */

/* --------------------------------------------------------------------------
 * Initialization
 * -------------------------------------------------------------------------- */

/*
 * Initialize device registry and all registered devices.
 * Must be called once at boot.
 */
void device_init(void);

/* --------------------------------------------------------------------------
 * Enumeration
 * -------------------------------------------------------------------------- */

/*
 * Begin enumeration of registered devices.
 *
 * Returns:
 *  - true  if at least one device exists, *out_id set
 *  - false if no devices are registered
 */
bool device_enum_first(uint8_t *out_id);

/*
 * Continue enumeration.
 *
 * Parameters:
 *  cur_id  - previously returned device ID
 *  out_id  - receives next device ID
 *
 * Returns:
 *  - true  if another device exists
 *  - false if no more devices
 */
bool device_enum_next(uint8_t cur_id, uint8_t *out_id);

/* --------------------------------------------------------------------------
 * Lookup
 * -------------------------------------------------------------------------- */

/*
 * Look up a device ID by name.
 *
 * Returns:
 *  - true  if found, *out_id set
 *  - false otherwise
 */
bool device_lookup_id(const char *name, uint8_t *out_id);

/* --------------------------------------------------------------------------
 * State access
 * -------------------------------------------------------------------------- */

/*
 * Set device state by ID.
 *
 * Returns:
 *  - true  if device exists and supports set_state
 *  - false otherwise
 */
bool device_set_state_by_id(uint8_t id, dev_state_t state);

 /*
  * Set device state from scheduler context.
  *
  * 'when' is the absolute UTC time of the governing schedule event.
  *
  * Returns:
  *  - true  if device exists and supports scheduled set
  *  - false otherwise
  */
 bool device_schedule_state_by_id(uint8_t id,
                                  dev_state_t state,
                                  uint32_t when);

 /*
 * Get device state by ID.
 *
 * Behavior:
 *  - If device exists but has no get_state(), returns DEV_STATE_UNKNOWN
 *
 * Returns:
 *  - true  if device exists
 *  - false if device ID invalid
 */
bool device_get_state_by_id(uint8_t id, dev_state_t *out_state);

/*
 * Get a human-readable state string.
 *
 * Returns:
 *  - true  if string available
 *  - false otherwise
 */
bool device_get_state_string(uint8_t id,
                             dev_state_t state,
                             const char **out_string);

/*
 * Get device name.
 *
 * Returns:
 *  - true  if device exists
 *  - false otherwise
 */
bool device_name(uint8_t id, const char **out_name);

/*
 * Parse a state argument for a device.
 *
 * Rules:
 *  - Prefer device-specific state_string()
 *  - Case-insensitive
 *  - Fallback to "on"/"off"
 */
bool device_parse_state_by_id(uint8_t id,
                              const char *arg,
                              dev_state_t *out);

/* --------------------------------------------------------------------------
 * Periodic service
 * -------------------------------------------------------------------------- */

/*
 * Call tick() on all registered devices that provide one.
 */
void device_tick(uint32_t now_ms);

/*
 * devices_busy()
 *
 * Returns true if any device state machine is currently active
 * and requires CPU time.
 *
 * Purpose:
 *   Prevent the main loop from entering sleep while
 *   actuators or timed sequences are in progress.
 *
 * Design Rules:
 *   - This function is the single authority for “system busy”.
 *   - main_firmware.cpp must NOT inspect individual devices.
 *   - Each device state machine is responsible for reporting
 *     its own active / idle status.
 *
 * Typical cases:
 *   - Door actuator is moving
 *   - Lock is energizing
 *   - Relay pulse in progress
 *   - Any timed mechanical action not yet complete
 *
 * Returns:
 *   true  → System must remain awake
 *   false → Safe to enter sleep
 */
bool devices_busy(void);


bool device_is_busy(uint8_t id);
