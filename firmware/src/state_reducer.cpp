/*
 * state_reducer.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose:
 *   Reduce declarative schedule events into the current expected device state.
 *
 * Design Model:
 *   - Backward-looking reducer.
 *   - Stateless and side-effect free.
 *   - No globals, no hardware access, no execution.
 *   - Pure function: inputs → facts.
 *
 * Concept:
 *   For each device, determine the most recent schedule event whose
 *   resolved time is <= now_minute. That event becomes the governing
 *   event for the device.
 *
 * Outputs (struct reduced_state):
 *   has_action[id]
 *       True if a governing event exists for this device.
 *
 *   action[id]
 *       The declarative action (ACTION_ON / ACTION_OFF) from the
 *       governing event.
 *
 *   when[id]
 *       Absolute Unix time (UTC) of the governing event.
 *
 *       This is the phase identity for the device.
 *       It allows higher layers to detect schedule transitions
 *       without comparing actions or minute-of-day values.
 *
 * Phase Identity:
 *   The 'when' field represents the epoch timestamp of the event
 *   that currently governs the device.
 *
 *   If 'when' changes between reducer runs, a schedule phase
 *   transition has occurred for that device.
 *
 *   This enables:
 *       - Manual override expiration on schedule bump
 *       - Deterministic phase tracking
 *       - Transition detection without minute-based polling hacks
 *
 * Time Model:
 *   - All scheduling is UTC.
 *   - now_minute is minute-of-day in UTC.
 *   - today_epoch_midnight must represent 00:00:00 UTC of the current day.
 *   - Event absolute time is:
 *
 *         when = today_epoch_midnight + minute * 60
 *
 *   No DST logic exists here. TZ/DST are UI concerns only.
 *
 * Safety:
 *   - Future events are ignored.
 *   - Sparse event tables are supported.
 *   - Only the latest event <= now wins.
 *
 * Updated:
 *   2026-02-19 — Added 'when' (epoch phase identity) to reduced_state.
 */

 #include <string.h>

#include "state_reducer.h"
#include "resolve_when.h"

void state_reducer_run(const Event *events,
                       size_t table_size,
                       const struct solar_times *sol,
                       uint16_t now_minute,
                       uint32_t today_epoch_midnight,
                       struct reduced_state *out)
{
    if (!events || !out)
        return;

    /* Clear output */
    memset(out, 0, sizeof(*out));

    uint16_t best_minute[STATE_REDUCER_MAX_DEVICES];
    bool     have_minute[STATE_REDUCER_MAX_DEVICES];

    memset(have_minute, 0, sizeof(have_minute));

    for (size_t i = 0; i < table_size; i++) {
        const Event *ev = &events[i];

        /* Skip unused slots */
        if (ev->refnum == 0)
            continue;

        if (ev->device_id >= STATE_REDUCER_MAX_DEVICES)
            continue;

        uint16_t minute;
        if (!resolve_when(&ev->when, sol, &minute))
            continue;

        /* Ignore future intent */
        if (minute > now_minute)
            continue;

        /* Latest event <= now wins */
        if (!have_minute[ev->device_id] ||
            minute >= best_minute[ev->device_id]) {

            best_minute[ev->device_id] = minute;
            out->action[ev->device_id] = ev->action;
            out->has_action[ev->device_id] = true;

             out->when[ev->device_id] =
                    today_epoch_midnight + ((uint32_t)minute * 60u);


            have_minute[ev->device_id] = true;
        }
    }
}
