/*
 * scheduler_reconcile.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Determine expected device state at current time
 *
 * Rules:
 *  - Pure reducer
 *  - No I/O
 *  - No globals
 *  - No execution
 *
 * Updated: 2026-01-01
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "events.h"

#define MAX_DEVICES 8   /* keep small, static */

struct scheduler_state {
    bool   has_action[MAX_DEVICES];
    Action action[MAX_DEVICES];
};

/* Compute expected device state at `now_minute` */
void scheduler_reconcile(const Event *events,
                         size_t count,
                         const struct solar_times *sol,
                         uint16_t now_minute,
                         struct scheduler_state *out);
