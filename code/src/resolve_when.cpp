/*
 * resolve_when.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Implementation of time resolution logic
 *
 * Updated: 2025-12-29
 */

#include "resolve_when.h"
#include "solar.h"

bool resolve_when(const struct When* when,
                  const struct solar_times* sol,
                  uint16_t* out_minute)
{
    if (!when || !out_minute) {
        return false;
    }

    int base = 0;

    switch (when->ref) {

    case REF_NONE:
        return false;

    case REF_MIDNIGHT:
        base = 0;
        break;

    case REF_SOLAR_STD:
        base = sol->sunrise_std;
        break;

    case REF_SOLAR_CIV:
        base = sol->sunrise_civ;
        break;

    default:
        return false;
    }

    int t = base + when->offset_minutes;

    /* No cross-midnight logic allowed */
    if (t < 0 || t >= 1440) {
        return false;
    }

    *out_minute = (uint16_t)t;
    return true;
}
