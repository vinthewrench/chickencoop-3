/*
 * scheduler_reconcile.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Scheduler state reducer (backward-looking)
 *
 * Updated: 2026-01-01
 */

#include "scheduler_reconcile.h"
#include "resolve_when.h"
#include <string.h>

void scheduler_reconcile(const Event *events,
                         size_t count,
                         const struct solar_times *sol,
                         uint16_t now_minute,
                         struct scheduler_state *out)
{
    if (!events || !out)
        return;

    /* Clear output */
    memset(out, 0, sizeof(*out));

    /* Track latest minute per device */
    uint16_t best_minute[MAX_DEVICES];
    bool     have_minute[MAX_DEVICES];

    memset(have_minute, 0, sizeof(have_minute));

    for (size_t i = 0; i < count; i++) {
        const Event *ev = &events[i];

        if (ev->device_id >= MAX_DEVICES)
            continue;

        uint16_t minute;
        if (!resolve_when(&ev->when, sol, &minute))
            continue;

        if (minute > now_minute)
            continue;

        if (!have_minute[ev->device_id] ||
            minute >= best_minute[ev->device_id]) {

            best_minute[ev->device_id] = minute;
            out->action[ev->device_id] = ev->action;
            out->has_action[ev->device_id] = true;
            have_minute[ev->device_id] = true;
        }
    }
}
