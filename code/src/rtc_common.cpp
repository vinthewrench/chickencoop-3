#include "rtc.h"

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
