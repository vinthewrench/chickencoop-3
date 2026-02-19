/*
 * resolve_when.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Resolve declarative time expressions to minute-of-day
 *
 * Rules:
 *  - Stateless, pure function
 *  - No RTC access
 *  - No device state
 *  - No cross-midnight wrapping
 *  - Invalid or unresolvable times return false
 *
 * Updated: 2025-12-30
 */

#include "resolve_when.h"
#include "solar.h"

bool resolve_when(const struct When* when,
                  const struct solar_times* sol,
                  uint16_t* out_minute)
{
    if (!when || !out_minute)
        return false;

    int32_t base;

    switch (when->ref) {

    case REF_NONE:
        return false;

    case REF_MIDNIGHT:
        base = 0;
        break;

    case REF_SOLAR_STD_RISE:
        if (!sol) return false;
        base = sol->sunrise_std;
        break;

    case REF_SOLAR_STD_SET:
        if (!sol) return false;
        base = sol->sunset_std;
        break;

    case REF_SOLAR_CIV_RISE:
        if (!sol) return false;
        base = sol->sunrise_civ;
        break;

    case REF_SOLAR_CIV_SET:
        if (!sol) return false;
        base = sol->sunset_civ;
        break;

    default:
        return false;
    }

    int32_t t = base + when->offset_minutes;

    /* Normalize to 0–1439 UTC (modular day) */
    t %= 1440;
    if (t < 0)
        t += 1440;

    *out_minute = (uint16_t)t;
    return true;
}
