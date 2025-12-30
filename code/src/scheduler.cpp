/*
 * scheduler.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Scheduler reducer
 *
 * Rules:
 *  - Events express intent only
 *  - No execution, no replay
 *  - Strictly minute > now
 *  - No cross-midnight logic
 *  - No persistent state
 *
 * Updated: 2025-12-30
 */

#include "scheduler.h"
#include "events.h"
#include "config_events.h"
#include "resolve_when.h"
#include "rtc.h"

void scheduler_run(scheduler_result_t *out)
{
    if (!out) {
        return;
    }

    uint16_t now = rtc_minutes_since_midnight();

    bool found = false;
    uint16_t next = 0;

    size_t evcount = 0;
    const Event *ev = config_events_get(&evcount);

    /* Reducer: find earliest minute > now */
    for (size_t i = 0; i < evcount; i++) {

        uint16_t m;

        if (!resolve_when(&ev[i].when,
                          nullptr,   /* solar not wired yet */
                          &m)) {
            continue;
        }

        if (m > now) {
            if (!found || m < next) {
                next = m;
                found = true;
            }
        }
    }

    if (found) {
        out->has_next = true;
        out->next_minute = next;
    } else {
        out->has_next = false;
        out->next_minute = 0;
    }
}
