/**
 * @file relay_device.cpp
 *
 * @brief Latching relay device implementation (relay1, relay2)
 *
 * Project: Chicken Coop Controller
 *
 * ---------------------------------------------------------------------------
 * PURPOSE
 * ---------------------------------------------------------------------------
 *
 * Implements two independent latching relay devices with support for:
 *
 *   - Immediate/manual control (set_state)
 *   - Scheduled control (schedule_state)
 *   - Automatic manual override protection
 *
 * Each relay tracks the timestamp of its last manual override.
 * Scheduler events that predate that override are ignored.
 *
 * ---------------------------------------------------------------------------
 * ARCHITECTURE
 * ---------------------------------------------------------------------------
 *
 * Control paths:
 *
 *   Manual / immediate control:
 *       relayX_set_state()
 *           → records override timestamp (UTC epoch)
 *           → calls relayX_set_state_internal()
 *
 *   Scheduled control:
 *       relayX_schedule_state(state, when)
 *           → ignores if when <= last_override_time
 *           → otherwise calls relayX_set_state_internal()
 *
 *   Hardware layer:
 *       relayX_set_state_internal()
 *           → updates cached state
 *           → drives latching relay coils
 *
 * ---------------------------------------------------------------------------
 * OVERRIDE MODEL
 * ---------------------------------------------------------------------------
 *
 * g_last_override_time marks the last manual intervention.
 *
 * Any scheduled event whose absolute timestamp ("when") is less than
 * or equal to that value is considered stale and ignored.
 *
 * The override automatically expires when a future schedule event
 * occurs (when > g_last_override_time).
 *
 * No boolean override flag is required.
 * Override lifetime is purely time-based and monotonic.
 *
 * ---------------------------------------------------------------------------
 * SAFETY
 * ---------------------------------------------------------------------------
 *
 * - Relays are initialized to OFF at boot.
 * - Duplicate state requests are ignored.
 * - Hardware calls are only issued on actual state transitions.
 *
 * ---------------------------------------------------------------------------
 * TIME MODEL
 * ---------------------------------------------------------------------------
 *
 * - All time comparisons use UTC epoch seconds.
 * - rtc_get_epoch() provides seconds since 2000-01-01 00:00:00 UTC.
 * - No timezone or DST logic exists at this layer.
 *
 * Updated: 2026-02-19
 */

#include "device.h"
#include "rtc.h"
#include "relay_hw.h"

/* ============================================================================
 * Internal State
 * ========================================================================== */

/** Cached logical state for relay1 */
static dev_state_t relay1_state = DEV_STATE_UNKNOWN;

/** Cached logical state for relay2 */
static dev_state_t relay2_state = DEV_STATE_UNKNOWN;

/** Timestamp of last manual override for relay1 */
static uint32_t relay1_last_override_time = 0;

/** Timestamp of last manual override for relay2 */
static uint32_t relay2_last_override_time = 0;


/* ============================================================================
 * Relay 1 Implementation
 * ========================================================================== */

/**
 * @brief Get current logical state of relay1.
 */
static dev_state_t relay1_get_state(void)
{
    return relay1_state;
}

/**
 * @brief Internal hardware state update for relay1.
 *
 * This function does NOT modify override time.
 */
static void relay1_set_state_internal(dev_state_t state)
{
    if (state == relay1_state)
        return;

    relay1_state = state;

    if (state == DEV_STATE_ON)
        relay1_set();
    else if (state == DEV_STATE_OFF)
        relay1_reset();
}

/**
 * @brief Manual/immediate state change for relay1.
 *
 * Records override timestamp.
 */
static void relay1_set_state(dev_state_t state)
{
    relay1_last_override_time = rtc_get_epoch();
    relay1_set_state_internal(state);
}

/**
 * @brief Scheduled state change for relay1.
 *
 * Ignores schedule events older than the last manual override.
 */
static void relay1_schedule_state(dev_state_t state, uint32_t when)
{
    if (when <= relay1_last_override_time)
        return;

    relay1_set_state_internal(state);
}


/* ============================================================================
 * Relay 2 Implementation
 * ========================================================================== */

/**
 * @brief Get current logical state of relay2.
 */
static dev_state_t relay2_get_state(void)
{
    return relay2_state;
}

/**
 * @brief Internal hardware state update for relay2.
 */
static void relay2_set_state_internal(dev_state_t state)
{
    if (state == relay2_state)
        return;

    relay2_state = state;

    if (state == DEV_STATE_ON)
        relay2_set();
    else if (state == DEV_STATE_OFF)
        relay2_reset();
}

/**
 * @brief Manual/immediate state change for relay2.
 */
static void relay2_set_state(dev_state_t state)
{
    relay2_last_override_time = rtc_get_epoch();
    relay2_set_state_internal(state);
}

/**
 * @brief Scheduled state change for relay2.
 *
 * Ignores stale schedule events.
 */
static void relay2_schedule_state(dev_state_t state, uint32_t when)
{
    if (when <= relay2_last_override_time)
        return;

    relay2_set_state_internal(state);
}


/* ============================================================================
 * Utilities
 * ========================================================================== */

/**
 * @brief Human-readable relay state string.
 */
static const char *relay_state_string(dev_state_t state)
{
    switch (state) {
    case DEV_STATE_ON:  return "ON";
    case DEV_STATE_OFF: return "OFF";
    default:            return "UNKNOWN";
    }
}


/* ============================================================================
 * Initialization
 * ========================================================================== */

/**
 * @brief Initialize relay hardware and force safe OFF state.
 *
 * Safe to call multiple times.
 */
static void relay_device_init(void)
{
    static uint8_t init = 0;
    if (init)
        return;

    relay_init();

    relay1_set_state(DEV_STATE_OFF);
    relay2_set_state(DEV_STATE_OFF);

    init = 1;
}


/* ============================================================================
 * Device Table Entries
 * ========================================================================== */

/** Relay1 device descriptor */
Device relay1_device = {
    .name           = "relay1",
    .deviceID       = DEVICE_ID_RELAY1,
    .init           = relay_device_init,
    .get_state      = relay1_get_state,
    .set_state      = relay1_set_state,
    .schedule_state = relay1_schedule_state,
    .state_string   = relay_state_string,
    .tick           = NULL,
    .is_busy        = NULL
};

/** Relay2 device descriptor */
Device relay2_device = {
    .name           = "relay2",
    .deviceID       = DEVICE_ID_RELAY2,
    .init           = relay_device_init,
    .get_state      = relay2_get_state,
    .set_state      = relay2_set_state,
    .schedule_state = relay2_schedule_state,
    .state_string   = relay_state_string,
    .tick           = NULL,
    .is_busy        = NULL
};
