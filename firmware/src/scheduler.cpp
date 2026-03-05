/* ============================================================================
 * scheduler.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Day-scoped scheduler logic
 *
 * Responsibilities:
 *  - Cache solar data for TODAY only
 *  - Answer “what is the next event minute today?”
 *  - Track schedule changes via an ETag
 *
 * Non-responsibilities:
 *  - No device execution
 *  - No RTC access
 *  - No config mutation
 *  - No timezone or DST logic
 *
 * Notes:
 *  - Reads config_events (sparse table)
 *  - Uses caller-supplied solar data
 *  - Global, single instance
 *
 * Updated: 2026-01-08
 * ========================================================================== */

#include "scheduler.h"
#include "config_events.h"
#include "resolve_when.h"

#include <string.h>

/* --------------------------------------------------------------------------
 * Global scheduler instance
 * -------------------------------------------------------------------------- */

struct scheduler_ctx g_scheduler;

/*
 * Schedule change token (ETag).
 *
 * Any modification to:
 *  - event definitions
 *  - solar inputs
 *  - date context
 *
 * MUST increment this value via schedule_touch().
 *
 * The main loop uses this to detect when it must
 * re-run reduction + apply immediately, even if
 * the minute has not changed.
 */
static uint32_t g_schedule_etag = 0;

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

void scheduler_init(void)
{
    /* Clear cached day/solar state */
    memset(&g_scheduler, 0, sizeof(g_scheduler));

    /* Reset schedule change token */
    g_schedule_etag = 0;
}

/*
 * Invalidate cached solar data.
 *
 * This is called when inputs to solar computation change
 * WITHOUT a date change (lat/lon, TZ, DST, manual date set).
 *
 * Effect:
 *  - Marks solar invalid
 *  - Forces recompute on next scheduler_update_day()
 *  - Touches schedule so main loop re-applies immediately
 */
void scheduler_invalidate_solar(void)
{
    if (g_scheduler.have_sol) {
        g_scheduler.have_sol = false;
        schedule_touch();
    }
}

/*
 * Update cached date and solar data for TODAY.
 *
 * This function does NOT compute solar.
 * It only records what the caller already computed.
 */
void scheduler_update_day(int y, int mo, int d,
                          const struct solar_times *sol,
                          bool have_sol)
{
    /*
     * If the calendar date is unchanged AND
     * solar validity did not change, this is a no-op.
     */
    if (g_scheduler.y == y &&
        g_scheduler.mo == mo &&
        g_scheduler.d == d &&
        g_scheduler.have_sol == have_sol)
        return;

    /* Cache new date */
    g_scheduler.y  = y;
    g_scheduler.mo = mo;
    g_scheduler.d  = d;

    /* Cache solar validity */
    g_scheduler.have_sol = have_sol;

    /* Copy solar data if provided */
    if (have_sol && sol)
        g_scheduler.sol = *sol;

    /*
     * Date or solar context changed.
     * This affects schedule resolution.
     */
    schedule_touch();
}

/* --------------------------------------------------------------------------
 * Schedule change tracking (ETag)
 * -------------------------------------------------------------------------- */

/*
 * Return current schedule ETag.
 *
 * The main loop compares this value to its last-seen
 * value to decide whether to re-run reduction + apply.
 */
uint32_t schedule_etag(void)
{
    return g_schedule_etag;
}

/*
 * Mark the schedule as changed.
 *
 * This is the ONLY cross-layer notification mechanism.
 * Console commands, config saves, and solar invalidation
 * must call this.
 */
void schedule_touch(void)
{
    g_schedule_etag++;
}

/* --------------------------------------------------------------------------
 * Queries
 * -------------------------------------------------------------------------- */

/*
 * Find the next scheduled event minute for TODAY.
 *
 * This is a pure query:
 *  - No side effects
 *  - No mutation
 */
bool scheduler_next_event_minute(uint16_t now_minute,
                                 uint16_t *out_minute)
{
    if (!out_minute)
        return false;

    now_minute %= 1440u;


    size_t used = 0;
    const Event *events = config_events_get(&used);

    if (!events)
        return false;

    bool found = false;
    uint16_t best = 0;

    for (size_t i = 0; i < MAX_EVENTS; i++) {

        const Event *ev = &events[i];

        if (ev->refnum == 0)
            continue;

        uint16_t minute;
        if (!resolve_when(&ev->when,
                          g_scheduler.have_sol ? &g_scheduler.sol : NULL,
                          &minute))
            continue;

        /* must be strictly in the future */
        if (minute <= now_minute)
            continue;

        if (!found || minute < best) {
            best = minute;
            found = true;
        }
    }

    /* if nothing left today, wrap to earliest tomorrow */

    if (!found) {

        for (size_t i = 0; i < MAX_EVENTS; i++) {

            const Event *ev = &events[i];

            if (ev->refnum == 0)
                continue;

            uint16_t minute;
            if (!resolve_when(&ev->when,
                              g_scheduler.have_sol ? &g_scheduler.sol : NULL,
                              &minute))
                continue;

            if (!found || minute < best) {
                best = minute;
                found = true;
            }
        }
    }

    if (!found)
        return false;

    *out_minute = best;
    return true;
}
