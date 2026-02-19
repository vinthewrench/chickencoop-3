/*
 * console_time.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */


 #include <stdbool.h>


#include "console_time.h"
#include "console_io.h"
#include "time_dst.h"

#include "rtc.h"

static void put_2d(int v)
{
    console_putc((char)('0' + (v / 10) % 10));
    console_putc((char)('0' + (v % 10)));
}

static void put_4d(int v)
{
    console_putc((char)('0' + (v / 1000) % 10));
    console_putc((char)('0' + (v / 100) % 10));
    console_putc((char)('0' + (v / 10) % 10));
    console_putc((char)('0' + (v % 10)));
}


void print_hhmm(uint16_t minutes)
{
    int h = minutes / 60;
    int mm = minutes % 60;
    bool pm = (h >= 12);

    if (h > 12)
        h -= 12;
    if (h == 0)
        h = 12;

    put_2d(h);
    console_putc(':');
    put_2d(mm);
    console_putc(' ');
    console_putc(pm ? 'P' : 'A');
    console_putc('M');
}


void print_datetime_ampm(int y,int mo,int d,int h,int m,int s)
{
    bool pm = (h >= 12);
    int h12 = h;

    if (h12 > 12)
        h12 -= 12;
    if (h12 == 0)
        h12 = 12;

    put_4d(y);
    console_putc('-');
    put_2d(mo);
    console_putc('-');
    put_2d(d);
    console_putc(' ');
    put_2d(h12);
    console_putc(':');
    put_2d(m);
    console_putc(':');
    put_2d(s);
    console_putc(' ');
    console_putc(pm ? 'P' : 'A');
    console_putc('M');

}

bool print_local_timedate() {
    int y, mo, d, h, m, s;

    if (!rtc_time_is_set()) {
        return false;
    }

    /* Read UTC */
    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    /* Convert UTC → LOCAL using minutes */
    int offset_min = utc_offset_minutes(y, mo, d, h);

    int total_min = h * 60 + m + offset_min;

    int yy = y;
    int mm2 = mo;
    int dd = d;

    /* Handle day rollover */
    while (total_min < 0) {
        total_min += 1440;
        dd--;
        if (dd < 1) {
            mm2--;
            if (mm2 < 1) {
                mm2 = 12;
                yy--;
            }
            dd = days_in_month(yy, mm2);
        }
    }

    while (total_min >= 1440) {
        total_min -= 1440;
        dd++;
        if (dd > days_in_month(yy, mm2)) {
            dd = 1;
            mm2++;
            if (mm2 > 12) {
                mm2 = 1;
                yy++;
            }
        }
    }

    int hh = total_min / 60;
    int mm = total_min % 60;

    print_datetime_ampm(yy, mm2, dd, hh, mm, s);

    return true;
}
