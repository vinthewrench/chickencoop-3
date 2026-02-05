/*
 * rtc.h
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC abstraction (host + firmware)
 *
 * Responsibilities:
 *  - Maintain wall-clock date/time
 *  - Provide deterministic access to current time
 *  - Support alarm scheduling for low-power operation
 *  - Expose minute-level time for scheduler reducer
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
 * Updated: 2026-01-05
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

/*
 * Claim RTC ownership at boot.
 *
 * This function performs no policy decisions.
 * It exists as a single, stable hook for future
 * RTC initialization once RUN mode is implemented.
 */
void rtc_init(void);

/*
 * Returns true if the RTC crystal oscillator is running.
 * This reflects the hardware oscillator state, not time validity.
 */
bool rtc_oscillator_running(void);

/*
 * Returns true if the stored time is considered valid.
 * This reflects the VL (voltage low) flag state.
 */
bool rtc_time_is_set(void);

/* --------------------------------------------------------------------------
 * Time API
 * -------------------------------------------------------------------------- */

void rtc_get_time(int *y, int *mo, int *d,
                  int *h, int *m, int *s);

bool rtc_set_time(int y, int mo, int d,
                  int h, int m, int s);

/* --------------------------------------------------------------------------
 * Alarm API
 * -------------------------------------------------------------------------- */

/*
 * Alarm interface (minute resolution is sufficient).
 * Alarm interrupt remains asserted until cleared.
 */
bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute);
void rtc_alarm_disable(void);
void rtc_alarm_clear_flag(void);

/* --------------------------------------------------------------------------
 * Scheduler support
 * -------------------------------------------------------------------------- */

/*
 * Returns minutes since midnight [0..1439].
 * Used by scheduler reducer logic.
 */
uint16_t rtc_minutes_since_midnight(void);

/* --------------------------------------------------------------------------
 * Scheduler helpers (platform-independent)
 * -------------------------------------------------------------------------- */

/*
 * Set an RTC alarm using minute-of-day [0..1439].
 *
 * Converts minute-of-day to HH:MM and arms the alarm.
 */
bool rtc_alarm_set_minute_of_day(uint16_t minute_of_day);
