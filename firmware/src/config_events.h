/* ============================================================================
 * config_events.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Event configuration storage API
 *
 * Notes:
 *  - Events are stored as a fixed-size SPARSE table
 *  - Slot is used iff events[i].refnum != 0
 *  - refnum is the stable external identifier
 *  - Callers must iterate 0..MAX_EVENTS-1 and skip unused slots
 *  - Scheduler treats the table as read-only
 *
 * Updated: 2026-01-08
 * ========================================================================== */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "events.h"

#define MAX_EVENTS 16
/* Accessor
 * Returns pointer to config-backed sparse event table (size MAX_EVENTS).
 * If count != NULL, *count is set to number of USED slots.
 *
 * IMPORTANT:
 *  - count is informational only
 *  - do NOT use it as an array bound
 */
const Event *config_events_get(size_t *count);

/* Mutators */
bool config_events_add(const Event *ev);                  /* allocates new refnum */
bool config_events_update_by_refnum(refnum_t ref,
                                    const Event *ev);    /* preserves refnum */
bool config_events_delete_by_refnum(refnum_t ref);

/* Utilities */
void config_events_clear(void);
