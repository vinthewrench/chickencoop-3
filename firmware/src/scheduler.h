/* ============================================================================
 * scheduler.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Day-scoped event scheduler (shared: host + firmware)
 *
 * What this IS:
 *  - Answers questions about TODAY only
 *  - Knows when the next scheduled event occurs (minute-of-day)
 *  - Caches solar data for the current day
 *  - Exposes a change token (ETag) for schedule invalidation
 *
 * What this is NOT:
 *  - No device execution
 *  - No RTC access
 *  - No config mutation
 *  - No timezone or DST logic
 *
 * Design rules:
 *  - Global, single instance
 *  - Deterministic
 *  - No dynamic allocation
 *
 * Updated: 2026-01-08
 * ========================================================================== */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "solar.h"

/* --------------------------------------------------------------------------
 * Scheduler runtime state (global)
 * -------------------------------------------------------------------------- */

/*
 * Cached context for "today".
 *
 * This is NOT the schedule definition.
 * This is only derived, day-scoped data.
 */
struct scheduler_ctx {
    int y, mo, d;               /* date solar cache applies to */
    struct solar_times sol;     /* cached solar times */
    bool have_sol;              /* false if solar unavailable/invalid */
};

/* Global scheduler instance */
extern struct scheduler_ctx g_scheduler;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

/*
 * Initialize scheduler state.
 *
 * - Called once at boot
 * - Clears cached date and solar data
 * - Resets internal change tracking
 */
void scheduler_init(void);

/*
 * Invalidate cached solar data.
 *
 * Call when inputs to solar calculation change:
 *  - latitude / longitude
 *  - timezone
 *  - DST policy
 *  - manual date set
 *
 * Effect:
 *  - Marks solar cache invalid
 *  - Forces recompute on next scheduler_update_day()
 *
 * NOTE:
 *  - Does NOT recompute immediately
 *  - Does NOT touch events
 */
void scheduler_invalidate_solar(void);

/*
 * Update scheduler day context.
 *
 * Caller supplies:
 *  - current calendar date
 *  - solar times for today (if available)
 *
 * Behavior:
 *  - If date unchanged AND solar still valid → no-op
 *  - Otherwise cache new date and solar state
 */
void scheduler_update_day(int y, int mo, int d,
                          const struct solar_times *sol,
                          bool have_sol);

/* --------------------------------------------------------------------------
 * Schedule change tracking (ETag)
 * -------------------------------------------------------------------------- */

/*
 * Return current schedule ETag.
 *
 * Meaning:
 *  - Any change to schedule definition OR its inputs
 *    increments this value.
 *
 * Used by:
 *  - main loop to decide whether to re-run reduction
 *
 * Notes:
 *  - Monotonic (wrap tolerated)
 *  - No timing semantics
 */
uint32_t schedule_etag(void);

/*
 * Mark the schedule as changed.
 *
 * Call this when ANY of the following change:
 *  - events added / deleted / cleared
 *  - config saved that affects scheduling
 *  - latitude / longitude
 *  - timezone or DST policy
 *  - manual date set
 *
 * Effect:
 *  - Increments internal ETag
 *  - Main loop will notice and re-apply schedule
 *
 * This is the ONLY cross-layer notification mechanism.
 */
void schedule_touch(void);

/* --------------------------------------------------------------------------
 * Queries
 * -------------------------------------------------------------------------- */

/*
 * Find the next scheduled event minute.
 *
 * Parameters:
 *   now_minute  - current UTC minute-of-day (0..1439)
 *
 * Returns:
 *   true  → out_minute set to the next event minute-of-day (0..1439)
 *   false → no valid events exist
 *
 * Behavior:
 *   - Finds the earliest event strictly after now_minute
 *   - If no future event exists today, wraps to the earliest event tomorrow
 *   - Ignores empty slots (refnum == 0)
 *   - Ignores events that fail resolve_when()
 *
 * Notes:
 *   - All times are UTC minute-of-day
 *   - Solar resolution uses cached scheduler solar context
 */
bool scheduler_next_event_minute(uint16_t now_minute,
                                 uint16_t *out_minute);
