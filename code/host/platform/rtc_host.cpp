#include "rtc.h"

#include <time.h>
#include <string.h>
#include <stdbool.h>

/*
 * Host RTC stub
 * - Always valid
 * - Initialized from host system time
 * - Can be overridden via rtc_set_time()
 * - After override, behaves like a real RTC (no resync)
 */

static bool g_rtc_valid  = true;
static bool g_rtc_manual = false;

static int g_year   = 0;
static int g_month  = 0;
static int g_day    = 0;
static int g_hour   = 0;
static int g_minute = 0;
static int g_second = 0;

/*
 * Host-side RTC initialization stub.
 * No hardware exists in the host build.
 */
void rtc_init(void)
{
    /* no-op */
}

/* Pull current local time from host */
static void rtc_sync_from_host(void)
{
    time_t now = time(NULL);
    struct tm lt;
    memset(&lt, 0, sizeof(lt));

#if defined(_POSIX_THREAD_SAFE_FUNCTIONS)
    localtime_r(&now, &lt);
#else
    struct tm *tmp = localtime(&now);
    if (!tmp)
        return;
    lt = *tmp;
#endif

    g_year   = lt.tm_year + 1900;
    g_month  = lt.tm_mon + 1;
    g_day    = lt.tm_mday;
    g_hour   = lt.tm_hour;
    g_minute = lt.tm_min;
    g_second = lt.tm_sec;
}

void rtc_get_time(int *year,
                  int *month,
                  int *day,
                  int *hour,
                  int *minute,
                  int *second)
{
    /* Caller bug protection */
    if (!year || !month || !day || !hour || !minute || !second)
        return;

    if (!g_rtc_manual)
        rtc_sync_from_host();

    *year   = g_year;
    *month  = g_month;
    *day    = g_day;
    *hour   = g_hour;
    *minute = g_minute;
    *second = g_second;
}

bool rtc_set_time(int y,int mo,int d,int h,int m,int s)
{
    /* Override RTC time (locks host sync) */
    g_year   = y;
    g_month  = mo;
    g_day    = d;
    g_hour   = h;
    g_minute = m;
    g_second = s;

    g_rtc_manual = true;
    g_rtc_valid  = true;
    return true;
}

bool rtc_time_is_set(void)
{
    return g_rtc_valid;
}

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
