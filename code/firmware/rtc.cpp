/*
 * rtc.cpp (firmware stub)
 *
 * Project: Chicken Coop Controller
 * Purpose: RTC stub implementation with alarm hooks
 *
 * Notes:
 *  - Temporary stub until PCF8523 hardware is wired
 *  - Provides linkable symbols for scheduler integration
 *  - No actual interrupts or alarms generated
 *
 * Updated: 2025-12-29
 */

#include "rtc.h"

static bool g_rtc_valid = false;

static int g_year   = 2025;
static int g_month  = 1;
static int g_day    = 1;
static int g_hour   = 12;
static int g_minute = 0;
static int g_second = 0;

void rtc_get_time(int *year,int *month,int *day,int *hour,int *minute,int *second)
{
    *year   = g_year;
    *month  = g_month;
    *day    = g_day;
    *hour   = g_hour;
    *minute = g_minute;
    *second = g_second;
}

void rtc_set_time(int y,int mo,int d,int h,int m,int s)
{
    g_year   = y;
    g_month  = mo;
    g_day    = d;
    g_hour   = h;
    g_minute = m;
    g_second = s;
    g_rtc_valid = true;
}

bool rtc_time_is_set(void)
{
    return g_rtc_valid;
}

/* ---- Alarm stubs ---- */

bool rtc_alarm_set_hm(uint8_t hour, uint8_t minute)
{
    (void)hour;
    (void)minute;
    return true;
}

void rtc_alarm_disable(void)
{
}

void rtc_alarm_clear_flag(void)
{
}
