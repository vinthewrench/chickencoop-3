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

#include "console_time.h"
#include "console_io.h"
#include <stdbool.h>


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

// bool parse_time(const char *s, int *h, int *m, int *sec)
// {
//     int hh = 0, mm = 0, ss = 0;
//     char ampm[3] = {0};

//     // Try strict 24-hour: HH:MM:SS
//     if (strlen(s) == 8 && s[2] == ':' && s[5] == ':') {
//         for (int i = 0; i < 8; i++) {
//             if (i == 2 || i == 5) continue;
//             if (s[i] < '0' || s[i] > '9')
//                 goto try_ampm;
//         }

//         hh = atoi(s);
//         mm = atoi(s + 3);
//         ss = atoi(s + 6);

//         if (hh < 0 || hh > 23) return false;
//         if (mm < 0 || mm > 59) return false;
//         if (ss < 0 || ss > 59) return false;

//         *h = hh; *m = mm; *sec = ss;
//         return true;
//     }

// try_ampm:
//     // HH:MM:SS AM|PM
//     if (sscanf(s, "%d:%d:%d %2s", &hh, &mm, &ss, ampm) != 4)
//         return false;

//     if (hh < 1 || hh > 12) return false;
//     if (mm < 0 || mm > 59) return false;
//     if (ss < 0 || ss > 59) return false;

//     bool pm;
//     if (ampm[0] == 'A' || ampm[0] == 'a')
//         pm = false;
//     else if (ampm[0] == 'P' || ampm[0] == 'p')
//         pm = true;
//     else
//         return false;

//     if (hh == 12)
//         hh = pm ? 12 : 0;
//     else if (pm)
//         hh += 12;

//     *h = hh; *m = mm; *sec = ss;
//     return true;
// }
