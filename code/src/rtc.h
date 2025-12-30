/*
 * rtc.h
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC abstraction (host + firmware)
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - Alarm support for power-down scheduling
 *
 * Updated: 2025-12-29
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Date convention:
 *   year  = full year (e.g. 2025)
 *   month = 1..12
 *   day   = 1..31
 */

void rtc_get_time(int*,int*,int*,int*,int*,int*);
void rtc_set_time(int y,int mo,int d,int h,int m,int s);
bool rtc_time_is_set(void);

/* Alarm interface (minute resolution is sufficient) */
bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute);
void rtc_alarm_disable(void);
void rtc_alarm_clear_flag(void);
