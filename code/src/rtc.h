/*
 * rtc.h
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC abstraction (host + firmware)
 *
 * Responsibilities:
 *  - Maintain wall-clock date/time (LOCAL civil time)
 *  - Provide deterministic access to current time
 *  - Support alarm scheduling for low-power operation
 *  - Provide UTC-normalized epoch helpers
 *
 * Time Model:
 *  - RTC stores LOCAL time.
 *  - Epoch helpers convert LOCAL → UTC.
 *  - Epoch base is 2000-01-01 00:00:00 UTC.
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Alarm support for power-down scheduling
 *
 * Hardware:
 *  - Chicken Coop Controller V3.0
 *  - RTC: NXP PCF8523
 *
 * Updated: 2026-02-06
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Date convention:
 *   year  = full year (e.g. 2026)
 *   month = 1..12
 *   day   = 1..31
 */

/* --------------------------------------------------------------------------
 * Bring-up / ownership
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize RTC hardware.
 *
 * Performs hardware-level initialization only.
 * Does not apply policy or scheduling decisions.
 */
void rtc_init(void);

/**
 * @brief Returns true if RTC crystal oscillator is running.
 *
 * Reflects hardware oscillator state only.
 */
bool rtc_oscillator_running(void);

/**
 * @brief Returns true if stored time is considered valid.
 *
 * Reflects hardware OS/VL status.
 */
bool rtc_time_is_set(void);

/* --------------------------------------------------------------------------
 * Time API (LOCAL civil time)
 * -------------------------------------------------------------------------- */

/**
 * @brief Read current LOCAL time from RTC.
 */
void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s);

/**
 * @brief Set LOCAL civil time in RTC.
 *
 * @return true on success.
 */
bool rtc_set_time(int y, int mo, int d,
                  int h, int m, int s);

/* --------------------------------------------------------------------------
 * Alarm API (minute resolution)
 * -------------------------------------------------------------------------- */

/**
 * @brief Set alarm using hour/minute match.
 *
 * Alarm interrupt remains asserted until cleared.
 */
bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute);

/**
 * @brief Disable RTC alarm interrupt.
 */
void rtc_alarm_disable(void);

/**
 * @brief Clear RTC alarm flag.
 *
 * Releases open-drain INT line.
 */
void rtc_alarm_clear_flag(void);

/* --------------------------------------------------------------------------
 * Scheduler Support
 * -------------------------------------------------------------------------- */

/**
 * @brief Returns minutes since midnight [0..1439].
 *
 * Derived from LOCAL RTC time.
 */
uint16_t rtc_minutes_since_midnight(void);

/**
 * @brief Set alarm using minute-of-day [0..1439].
 *
 * Converts minute-of-day → HH:MM.
 * Caller must ensure the minute is in the future.
 */
bool rtc_alarm_set_minute_of_day(uint16_t minute_of_day);

/* --------------------------------------------------------------------------
 * Epoch Helpers (UTC-normalized, 2000 base)
 * -------------------------------------------------------------------------- */

/**
 * @brief Get current UTC epoch seconds.
 *
 * Returns seconds since:
 *   2000-01-01 00:00:00 UTC
 *
 * Behavior:
 *  - Reads LOCAL time from RTC.
 *  - Applies configured timezone (g_cfg.tz).
 *  - Applies DST if enabled (g_cfg.honor_dst).
 *  - Returns normalized UTC seconds.
 *
 * Returns 0 if RTC is invalid.
 */
uint32_t rtc_get_epoch(void);

/**
 * @brief Convert calendar date/time to UTC epoch seconds.
 *
 * @param y          Year (e.g. 2026)
 * @param mo         Month [1..12]
 * @param d          Day [1..31]
 * @param h          Hour [0..23]
 * @param m          Minute [0..59]
 * @param s          Second [0..59]
 * @param tz_hours   Timezone offset from UTC
 * @param honor_dst  Apply US DST rule if true
 *
 * @return Seconds since 2000-01-01 00:00:00 UTC.
 *
 * Notes:
 *  - Input time is interpreted as LOCAL civil time.
 *  - Output is UTC-normalized.
 *  - Deterministic, no hardware access.
 *  - Valid for years >= 2000.
 */
uint32_t rtc_epoch_from_ymdhms(
    int y, int mo, int d,
    int h, int m, int s,
    int tz_hours,
    bool honor_dst);
