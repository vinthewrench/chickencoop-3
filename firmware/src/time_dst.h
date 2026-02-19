#pragma once

#include <stdbool.h>

/*
 * Determine whether US Daylight Saving Time is in effect.
 *
 * Rules (since 2007):
 *  - Starts second Sunday in March at 02:00 local time
 *  - Ends first Sunday in November at 02:00 local time
 *
 * Parameters:
 *  y  - year (e.g. 2025)
 *  m  - month (1–12)
 *  d  - day (1–31)
 *  h  - hour (0–23)
 *
 * Returns:
 *  true  if DST is active
 *  false otherwise
 */
bool is_us_dst(int y, int m, int d, int h);


int utc_offset_minutes(int y, int mo, int d, int h);

bool is_leap_year(int y);

int days_in_month(int y, int mo);
