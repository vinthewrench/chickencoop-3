/*
 * config_common.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Shared configuration defaults
 *
 * Notes:
 *  - Used by both host and firmware
 *  - Must not include AVR- or platform-specific headers
 *  - All fields initialized explicitly
 *
 * Updated: 2025-12-30
 */

#include "config.h"
#include <string.h>

/* Global runtime configuration */
struct config g_cfg;
/*
 * Fletcher-16 checksum
 * Used for config persistence integrity (host + AVR)
 */
uint16_t config_fletcher16(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t sum1 = 0;
    uint16_t sum2 = 0;

    while (len--) {
        sum1 = (sum1 + *p++) % 255;
        sum2 = (sum2 + sum1) % 255;
    }

    return (sum2 << 8) | sum1;
}

void config_defaults(struct config *cfg)
{
    /* Start from a known baseline */
    memset(cfg, 0, sizeof(*cfg));

    /* ---- Time / location defaults ---- */

    cfg->tz        = -6;   /* CST */
    cfg->honor_dst = 1;
    cfg->rtc_set_epoch = 0;     // last time we set the clock

    /* 34.4653°, -93.3628° */
    cfg->latitude_e4  =  344653;
    cfg->longitude_e4 = -933628;

    /* ---- Mechanical timing defaults ---- */

      cfg->door_travel_ms = 8000;   /* 8 seconds full open/close */
      cfg->lock_pulse_ms  = 500;    /* 500 ms solenoid pulse */

      cfg->door_settle_ms = 2000;   /* allow gravity + obstruction to clear */
      cfg->lock_settle_ms = 500;    /* time after unlock before motion */

    /* ---- Any future fields MUST be initialized here ---- */
}
