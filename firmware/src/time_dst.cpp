/*
 * time_dst.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: US DST computation helper
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#include <stdbool.h>
#include "time_dst.h"
#include "config.h"

/*
 * US Daylight Saving Time determination.
 * Rules (since 2007):
 *  - Starts second Sunday in March at 02:00
 *  - Ends first Sunday in November at 02:00
 */

static int day_of_week(int y, int m, int d)
{
    /* Zeller's congruence, 0=Sunday */
    if (m < 3) { m += 12; y--; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + 13*(m + 1)/5 + K + K/4 + J/4 + 5*J) % 7;
    return (h + 6) % 7;
}

static int nth_sunday(int y, int m, int n)
{
    int d = 1;
    int dow = day_of_week(y, m, d);
    int first_sunday = (dow == 0) ? 1 : (8 - dow);
    return first_sunday + (n - 1) * 7;
}

bool is_us_dst(int y, int m, int d, int h)
{
    if (m < 3 || m > 11) return false;
    if (m > 3 && m < 11) return true;

    if (m == 3) {
        int start = nth_sunday(y, 3, 2);
        if (d > start) return true;
        if (d < start) return false;
        return h >= 2;
    }

    if (m == 11) {
        int end = nth_sunday(y, 11, 1);
        if (d < end) return true;
        if (d > end) return false;
        return h < 2;
    }

    return false;
}



int utc_offset_minutes(int y, int mo, int d, int h)
{
    int dst = 0;

    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        dst = 60;

    return g_cfg.tz * 60 + dst;
}



/* -------------------------------------------------------------------------- */
/* Date math                                                                  */
/* -------------------------------------------------------------------------- */

bool is_leap_year(int y)
{
    if ((y % 400) == 0) return true;
    if ((y % 100) == 0) return false;
    return (y % 4) == 0;
}

int days_in_month(int y, int mo)
{
    static const uint8_t dpm[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};

    if (mo < 1 || mo > 12) return 31;
    if (mo == 2 && is_leap_year(y)) return 29;
    return dpm[mo - 1];
}
