/*
 * rtc_common.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Platform-independent RTC helper functions
 *
 * Responsibilities:
 *  - Provide derived time values based on current RTC state
 *  - Contain no hardware-specific logic
 *  - Behave identically on host and firmware builds
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Uses rtc.h API only
 *
 * Updated: 2026-01-05
 */

#include "rtc.h"
#include <stdint.h>

/*
 * Returns minutes since midnight [0..1439].
 *
 * This helper is intentionally defensive:
 *  - Clamps out-of-range values
 *  - Avoids propagating invalid RTC data into scheduler logic
 *
 * No hardware access occurs here.
 */
uint16_t rtc_minutes_since_midnight(void)
{
    int y, mo, d, h, m, s;

    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    if (h < 0)   h = 0;
    if (h > 23)  h = 23;
    if (m < 0)   m = 0;
    if (m > 59)  m = 59;

    return (uint16_t)(h * 60 + m);
}
