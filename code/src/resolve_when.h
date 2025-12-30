/*
 * resolve_when.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Resolve declarative time expressions to minute-of-day
 *
 * Notes:
 *  - Stateless, pure function
 *  - No dependency on device state
 *  - Invalid times are rejected, never wrapped
 *
 * Updated: 2025-12-29
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "events.h"

struct solar_times;

/* Resolve a When expression into minute-of-day.
 * Returns true on success, false if disabled or out-of-range.
 */
bool resolve_when(const struct When* when,
                  const struct solar_times* sol,
                  uint16_t* out_minute);
