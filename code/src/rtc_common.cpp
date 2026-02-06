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
 * Time Model:
 *  - RTC stores LOCAL civil time.
 *  - Epoch functions convert LOCAL time → UTC.
 *  - Epoch base is 2000-01-01 00:00:00 UTC.
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Uses rtc.h API only
 *
 * Updated: 2026-02-06
 */

#include <stdint.h>
#include <stdbool.h>

#include "rtc.h"
#include "config.h"
#include "time_dst.h"

/* --------------------------------------------------------------------------
 * Minutes Since Midnight
 * -------------------------------------------------------------------------- */

/**
 * @brief Return minutes since midnight [0..1439].
 *
 * Defensive helper:
 *  - Clamps invalid hour/minute values.
 *  - Prevents corrupt RTC data from propagating into scheduler logic.
 *
 * No hardware access beyond rtc_get_time().
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

/* --------------------------------------------------------------------------
 * Alarm Helper
 * -------------------------------------------------------------------------- */

/**
 * @brief Program RTC alarm for a minute-of-day.
 *
 * @param minute_of_day Minute index [0..1439]
 *
 * Converts minute-of-day to HH:MM and arms the RTC alarm.
 *
 * Notes:
 *  - Alarm is assumed to be for TODAY.
 *  - Caller must ensure minute_of_day is in the future.
 *  - Does not handle wrap-to-tomorrow logic.
 *  - Clears any pending alarm flag before arming.
 */
bool rtc_alarm_set_minute_of_day(uint16_t minute_of_day)
{
    if (minute_of_day >= 1440)
        return false;

    uint8_t h = (uint8_t)(minute_of_day / 60);
    uint8_t m = (uint8_t)(minute_of_day % 60);

    rtc_alarm_clear_flag();
    return rtc_alarm_set_hm(h, m);
}

/* --------------------------------------------------------------------------
 * Calendar Helpers
 * -------------------------------------------------------------------------- */

static bool is_leap_year(int y)
{
    if ((y % 400) == 0) return true;
    if ((y % 100) == 0) return false;
    return (y % 4) == 0;
}

static int days_in_month(int y, int mo)
{
    static const uint8_t dpm[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};

    if (mo < 1 || mo > 12)
        return 31;

    if (mo == 2 && is_leap_year(y))
        return 29;

    return dpm[mo - 1];
}

/* --------------------------------------------------------------------------
 * Epoch Conversion
 * -------------------------------------------------------------------------- */

/**
 * @brief Convert calendar date/time to UTC epoch seconds.
 *
 * @param y          Year (full year, e.g. 2026)
 * @param mo         Month [1..12]
 * @param d          Day [1..31]
 * @param h          Hour [0..23]
 * @param m          Minute [0..59]
 * @param s          Second [0..59]
 * @param tz_hours   Timezone offset from UTC in hours
 * @param honor_dst  If true, apply US DST rules via is_us_dst()
 *
 * @return Seconds since 2000-01-01 00:00:00 UTC.
 *
 * Description:
 *  - Treats input time as LOCAL civil time.
 *  - Converts calendar date to days since 2000-01-01.
 *  - Converts HH:MM:SS to seconds-of-day.
 *  - Applies timezone offset and optional DST correction.
 *  - Returns normalized UTC seconds.
 *
 * Design Constraints:
 *  - Deterministic, no hardware access.
 *  - Valid for years >= 2000.
 *  - 32-bit safe through year 2136.
 *  - Caller must supply valid calendar values.
 */

 #define UNIX_EPOCH_OFFSET_2000 946684800UL  /* seconds from 1970 to 2000 */

 uint32_t rtc_epoch_from_ymdhms(
     int y, int mo, int d,
     int h, int m, int s,
     int tz_hours,
     bool honor_dst
 )
 {
     uint32_t days = 0;

     for (int year = 2000; year < y; year++) {
         days += is_leap_year(year) ? 366u : 365u;
     }

     for (int month = 1; month < mo; month++) {
         days += (uint32_t)days_in_month(y, month);
     }

     days += (uint32_t)(d - 1);

     uint32_t seconds = days * 86400u;
     seconds += (uint32_t)h * 3600u;
     seconds += (uint32_t)m * 60u;
     seconds += (uint32_t)s;

     /* Convert LOCAL → UTC */
     int offset = tz_hours;

     if (honor_dst && is_us_dst(y, mo, d, h)) {
         offset += 1;
     }

     seconds -= (uint32_t)offset * 3600u;

     /* Convert 2000-based → Unix epoch (1970-based) */
     seconds += UNIX_EPOCH_OFFSET_2000;

     return seconds;
 }

/**
 * @brief Get current epoch derived from RTC.
 *
 * @return Seconds since 2000-01-01 00:00:00 UTC.
 *
 * Behavior:
 *  - Reads current LOCAL time from RTC.
 *  - Applies configured timezone (g_cfg.tz).
 *  - Applies DST if enabled (g_cfg.honor_dst).
 *  - Returns UTC-normalized seconds.
 *
 * Returns 0 if RTC is not set.
 */
uint32_t rtc_get_epoch(void)
{
    if (!rtc_time_is_set())
        return 0;

    int y, mo, d, h, m, s;
    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    return rtc_epoch_from_ymdhms(
        y, mo, d, h, m, s,
        g_cfg.tz,
        g_cfg.honor_dst
    );
}
