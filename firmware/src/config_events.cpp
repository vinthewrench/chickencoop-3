/**
 * @file config_events.cpp
 * @brief Persistent storage for declarative schedule events (sparse table).
 *
 * @details
 * This module owns the persistent event table (`g_cfg.events`) and is the sole
 * authority for schedule intent. The table is sparse: unused slots exist and
 * must be skipped by callers.
 *
 * @par Invariants
 * - `refnum != 0` is the sole indicator of an active slot.
 * - Inactive slots are fully zeroed (`refnum == 0` implies all fields cleared).
 *
 * @par Scheduler contract
 * Any mutation of the event table MUST call `schedule_touch()` to invalidate
 * scheduler caches and next-event reductions.
 *
 * Updated: 2026-01-08
 */

#include <string.h>

#include "config_events.h"
#include "config.h"
#include "scheduler.h"   /* schedule_touch() */


/**
 * @brief Returns a pointer to the full sparse event table.
 *
 * @param[out] count Optional. Receives the number of active events (`refnum != 0`).
 *
 * @return Pointer to the full table (size `MAX_EVENTS`).
 *
 * @note
 * The returned table contains unused slots. Callers MUST scan `MAX_EVENTS`
 * entries and skip any slot where `refnum == 0`.
 *
 * @warning
 * This function is read-only and MUST NOT have side effects. In particular,
 * it MUST NOT call `schedule_touch()`.
 */
const Event *config_events_get(size_t *count)
{
    size_t n = 0;

    for (size_t i = 0; i < MAX_EVENTS; i++) {
        if (g_cfg.events[i].refnum != 0)
            n++;
    }

    if (count)
        *count = n;

    return g_cfg.events;
}


/**
 * @brief Adds a new event to the first free slot in the sparse table.
 *
 * @param[in] ev Event definition to insert.
 *
 * @retval true  Event inserted successfully.
 * @retval false `ev` is NULL or the table is full.
 *
 * @details
 * Assigns a stable identity (`refnum`) to the inserted event. The `refnum`
 * is computed as `(index + 1)` and is guaranteed non-zero.
 *
 * @note
 * This function mutates schedule intent and MUST call `schedule_touch()`.
 */
bool config_events_add(const Event *ev)
{
    if (!ev)
        return false;

    for (size_t i = 0; i < MAX_EVENTS; i++) {
        if (g_cfg.events[i].refnum == 0) {

            /* Copy full event definition into empty slot. */
            g_cfg.events[i] = *ev;

            /* Assign stable identity. */
            g_cfg.events[i].refnum = (refnum_t)(i + 1);

            /* Schedule definition changed. */
            schedule_touch();

            return true;
        }
    }

    return false; /* table full */
}


/**
 * @brief Updates an existing event selected by its refnum.
 *
 * @param[in] ref Stable identity of the event to update (must be non-zero).
 * @param[in] ev  New event definition.
 *
 * @retval true  Event updated.
 * @retval false Invalid arguments or `ref` not found.
 *
 * @details
 * The event identity (`refnum`) is preserved across update.
 *
 * @note
 * This function mutates schedule intent and MUST call `schedule_touch()`.
 */
bool config_events_update_by_refnum(refnum_t ref, const Event *ev)
{
    if (!ev || ref == 0)
        return false;

    for (size_t i = 0; i < MAX_EVENTS; i++) {
        if (g_cfg.events[i].refnum == ref) {

            g_cfg.events[i] = *ev;
            g_cfg.events[i].refnum = ref;

            schedule_touch();
            return true;
        }
    }

    return false;
}


/**
 * @brief Deletes an event selected by its refnum.
 *
 * @param[in] ref Stable identity of the event to delete (must be non-zero).
 *
 * @retval true  Event deleted.
 * @retval false `ref` invalid or not found.
 *
 * @details
 * The slot is fully cleared to preserve the invariant that inactive slots are
 * zeroed (`refnum == 0` implies all fields cleared). The slot may be reused by
 * future inserts.
 *
 * @note
 * This function mutates schedule intent and MUST call `schedule_touch()`.
 */
bool config_events_delete_by_refnum(refnum_t ref)
{
    if (ref == 0)
        return false;

    for (size_t i = 0; i < MAX_EVENTS; i++) {
        if (g_cfg.events[i].refnum == ref) {

            /* Fully clear slot to preserve inactive-slot invariant. */
            memset(&g_cfg.events[i], 0, sizeof(g_cfg.events[i]));

            schedule_touch();
            return true;
        }
    }

    return false;
}


/**
 * @brief Clears all schedule events.
 *
 * @details
 * Fully clears all slots in the sparse table. After return, all `refnum` values
 * are zero and all event fields are cleared.
 *
 * @note
 * This function mutates schedule intent and MUST call `schedule_touch()` once.
 */
void config_events_clear(void)
{
    /* Zero entire table to preserve inactive-slot invariant. */
    memset(g_cfg.events, 0, sizeof(g_cfg.events));

    schedule_touch();
}
