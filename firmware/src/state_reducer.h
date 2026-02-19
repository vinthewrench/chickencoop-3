/*
 * state_reducer.h
 *
 * Project: Chicken Coop Controller
 * Purpose:
 *   Reduce declarative schedule events into a device-centric view
 *   of current scheduler intent.
 *
 * Design Rules:
 *  - Pure reducer (no side effects)
 *  - No I/O
 *  - No globals
 *  - No execution or replay
 *  - Deterministic and backward-looking
 *
 * Behavioral Model:
 *  - For each device ID, the reducer finds the most recent
 *    event whose resolved minute-of-day is <= now_minute.
 *  - That event becomes the governing event for the device.
 *  - Future events are ignored.
 *
 * Properties:
 *  - Safe to call at boot
 *  - Safe after RTC set
 *  - Safe after crash/restart
 *  - Works with sparse event tables
 *
 * Phase Identity:
 *  - Each device output includes the absolute UTC timestamp
 *    ("when") of the governing event.
 *  - If 'when' changes between reducer runs, the schedule
 *    phase for that device has changed.
 *  - Higher layers may use this for override clearing,
 *    transition detection, or auditing.
 *
 * Time Model:
 *  - Scheduling is UTC only.
 *  - now_minute is minute-of-day in UTC.
 *  - Absolute event time is computed as:
 *
 *        when = today_epoch_midnight + (minute * 60)
 *
 *  - DST/TZ adjustments are NOT part of this layer.
 *
 * Updated:
 *   2026-02-19 — Added epoch "when" phase identity per device.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "events.h"
#include "solar.h"

/* Must cover all possible device IDs */
#define STATE_REDUCER_MAX_DEVICES 8

/*
 * Device-centric reduced scheduler intent.
 *
 * One slot per device ID.
 *
 * has_action[id]
 *     True if a governing event exists for this device.
 *
 * action[id]
 *     Declarative action (ACTION_ON / ACTION_OFF)
 *     from the governing event.
 *
 * when[id]
 *     Absolute UTC Unix timestamp of the governing event.
 *     This is the device's schedule phase identity.
 */
struct reduced_state {
    bool     has_action[STATE_REDUCER_MAX_DEVICES];
    Action   action[STATE_REDUCER_MAX_DEVICES];
    uint32_t when[STATE_REDUCER_MAX_DEVICES];
};

/*
 * Reduce events into expected device state at `now_minute`.
 *
 * Parameters:
 *  events      - sparse declarative event table
 *  table_size  - total table size (MAX_EVENTS)
 *  sol         - resolved solar times for today (may be NULL)
 *  now_minute  - current minute-of-day (0..1439), UTC
 *  out         - output reduced state (cleared internally)
 *
 * Contract:
 *  - out is fully zeroed before use.
 *  - Only latest event <= now_minute per device is retained.
 *  - No hardware is touched.
 */
void state_reducer_run(const Event *events,
                       size_t table_size,
                       const struct solar_times *sol,
                       uint16_t now_minute,
                       uint32_t today_epoch_midnight,
                       struct reduced_state *out);
