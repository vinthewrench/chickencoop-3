
/*
 * console_cmds.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Console command definitions and dispatch
 *
 * Design notes:
 *  - This file defines the interactive CONFIG/console command set.
 *  - Command metadata (names, help text, argument limits) is treated as
 *    read-only data and MUST NOT consume SRAM on AVR targets.
 *
 * Memory model:
 *  - On AVR builds, all command strings and the command table itself are
 *    placed in flash (PROGMEM) to preserve scarce SRAM.
 *
 * Implementation strategy:
 *  - An X-macro (CMD_LIST) is used as the single source of truth for
 *    command definitions.
 *  - That list is expanded twice:
 *      1) To emit named string objects (flash on AVR, RAM on host)
 *      2) To emit the command table itself
 *  - This avoids illegal use of PSTR() in C++ global initializers while
 *    keeping behavior identical across platforms.
 *
 * Constraints:
 *  - No dynamic allocation
 *  - Deterministic behavior
 *  - Offline operation
 *  - AVR SRAM is a hard limit, not a suggestion
 *
 * Updated: 2025-12-31
 */

 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>
 #include <util/delay.h>


#include "console/console_io.h"
#include "console/console.h"
#include "console/mini_printf.h"
#include "time_dst.h"
#include "console_time.h"
#include "events.h"
#include "config_events.h"
#include "resolve_when.h"
#include "next_event.h"

#include "solar.h"
#include "rtc.h"
#include "config.h"
#include "uptime.h"
#include "scheduler.h"
#include  "door_lock.h"
#include "devices/devices.h"
#include "devices/led_state_machine.h"
#include "devices/door_state_machine.h"
#include "state_reducer.h"
#include "system_sleep.h"

#define DOOR_SW_BIT     PD3
#define RTC_INT_BIT     PD2


static inline void gpio_rtc_int_input_init(void)
{
    /* RTC_INT uses external pull-up */
    DDRD  &= (uint8_t)~(1u << RTC_INT_BIT);
    PORTD &= (uint8_t)~(1u << RTC_INT_BIT);
}

static inline void gpio_door_sw_input_init(void)
{
    /* Door switch uses internal pull-up */
    DDRD  &= (uint8_t)~(1u << DOOR_SW_BIT);
    PORTD |=  (uint8_t)(1u << DOOR_SW_BIT);
}

static inline uint8_t gpio_rtc_int_is_asserted(void)
{
    return (PIND & (1u << RTC_INT_BIT)) == 0u;
}

static inline uint8_t gpio_door_sw_is_asserted(void)
{
    return (PIND & (1u << DOOR_SW_BIT)) == 0u;
}


// -----------------------------------------------------------------------------
// CONFIG shadow state
//   - set commands modify RAM shadow only
//   - save commits to EEPROM and programs RTC
// -----------------------------------------------------------------------------

static bool g_cfg_loaded = false;
static bool g_cfg_dirty = false;


/* Command handler forward declarations */
static void console_help(int argc, char **argv);
static void cmd_version(int argc, char **argv);
static void cmd_time(int argc, char **argv);
static void cmd_schedule(int argc, char **argv);
static void cmd_solar(int argc, char **argv);
static void cmd_set(int argc, char **argv);
static void cmd_config(int argc, char **argv);
static void cmd_save(int argc, char **argv);
static void cmd_door(int argc, char **argv);
static void cmd_lock(int argc, char **argv);
static void cmd_event(int argc, char **argv);
static void cmd_sleep(int argc, char **argv);


// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

static void str_to_lower(char *s)
{
    while (*s) {
        *s = (char)tolower((unsigned char)*s);
        ++s;
    }
}

static void ensure_cfg_loaded(void)
{
    if (g_cfg_loaded)
        return;

    config_load(&g_cfg);
    g_cfg_loaded = true;
}


/* -------------------------------------------------------------------------- */
/* Date math                                                                  */
/* -------------------------------------------------------------------------- */

static bool is_leap_year(int y)
{
    if ((y % 400) == 0) return true;
    if ((y % 100) == 0) return false;
    return (y % 4) == 0;
}

static int days_in_month(int y, int mo)
{
    static const uint8_t dpm[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};

    if (mo < 1 || mo > 12) return 31;
    if (mo == 2 && is_leap_year(y)) return 29;
    return dpm[mo - 1];
}

/* -------------------------------------------------------------------------- */
/* Parsing helpers                                                            */
/* -------------------------------------------------------------------------- */

static bool parse_date_ymd(const char *s, int *y, int *mo, int *d)
{
    if (!s || strlen(s) != 10) return false;
    if (s[4] != '-' || s[7] != '-') return false;

    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }

    *y  = atoi(s);
    *mo = atoi(s + 5);
    *d  = atoi(s + 8);

    if (*y < 2000 || *y > 2099) return false;
    if (*mo < 1 || *mo > 12)   return false;
    if (*d < 1 || *d > days_in_month(*y, *mo)) return false;

    return true;
}

/* HH:MM (24-hour) — used for door rules */
static bool parse_time_hm(const char *s, int *h, int *m)
{
    if (!s || strlen(s) != 5) return false;
    if (s[2] != ':') return false;

    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }

    *h = atoi(s);
    *m = atoi(s + 3);

    if (*h < 0 || *h > 23) return false;
    if (*m < 0 || *m > 59) return false;

    return true;
}


static bool parse_time_hms(const char *s,int *h,int *m,int *sec)
{
    // HH:MM:SS (24-hour)
    if (!s || strlen(s) != 8) return false;
    if (s[2] != ':' || s[5] != ':') return false;
    for (int i = 0; i < 8; i++) {
        if (i == 2 || i == 5) continue;
        if (s[i] < '0' || s[i] > '9') return false;
    }
    *h   = atoi(s);
    *m   = atoi(s + 3);
    *sec = atoi(s + 6);
    if (*h < 0 || *h > 23) return false;
    if (*m < 0 || *m > 59) return false;
    if (*sec < 0 || *sec > 59) return false;
    return true;
}

static void print_uint_padded(unsigned v, size_t width)
{
    mini_printf("%u", v);

    /* count digits */
    unsigned n = 1;
    unsigned t = v;
    while (t >= 10) {
        t /= 10;
        n++;
    }

    while (n++ < width)
        console_putc(' ');
}

static void print_padded(const char *s, size_t width)
{
    if (!s)
        s = "?";

    size_t n = strlen(s);
    console_puts(s);

    while (n++ < width)
        console_putc(' ');
}


static void when_print(const struct When *w)
{
    if (!w) {
        console_puts("?");
        return;
    }

    int off = w->offset_minutes;
    char sign = (off < 0) ? '-' : '+';
    int mins = abs(off);

    switch (w->ref) {

    case REF_NONE:
        console_puts("DISABLED");
        return;

    case REF_MIDNIGHT: {
        int h = off / 60;
        int m = abs(off % 60);
        mini_printf("%02d:%02d", h, m);
        return;
    }

    case REF_SOLAR_STD_RISE:
        mini_printf("Sunrise %c%d", sign, mins);
        return;

    case REF_SOLAR_STD_SET:
        mini_printf("Sunset %c%d", sign, mins);
        return;

    case REF_SOLAR_CIV_RISE:
        mini_printf("Dawn %c%d", sign, mins);
        return;

    case REF_SOLAR_CIV_SET:
        mini_printf("Dusk %c%d", sign, mins);
        return;

    default:
        console_puts("?");
        return;
    }
}



static bool parse_signed_int(const char *s, int *out)
{
    if (!s || !*s)
        return false;

    char *end = NULL;
    long v = strtol(s, &end, 10);

    if (*end != '\0')
        return false;

    if (v < -32768 || v > 32767)
        return false;

    *out = (int)v;
    return true;
}

static bool compute_today_solar(struct solar_times *out)
{
    if (!out)
        return false;

    int y, mo, d, h;

    if (!rtc_time_is_set())
        return false;

    /* RTC now returns UTC */
    rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

    double lat = (double)g_cfg.latitude_e4 / 10000.0;
    double lon = (double)g_cfg.longitude_e4 / 10000.0;

    /*
     * Scheduling must be DST-invariant.
     * Always request solar times in UTC.
     */
    return solar_compute(
        y,
        mo,
        d,
        lat,
        lon,
        0,
        out
    );
}

// -----------------------------------------------------------------------------
// Commands
// -----------------------------------------------------------------------------

static void cmd_version(int,char**)
{
    console_puts("Chicken Coop Controller ");
    console_puts(PROJECT_VERSION);
    console_puts(" (");
    console_puts(__DATE__);
    console_puts(" ");
    console_puts(__TIME__);
    console_puts(")\n");
}

static void cmd_time(int, char **)
{
    int y, mo, d, h, m, s;

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    /* Read UTC */
    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    /* Convert UTC → LOCAL for display */
    int tz = g_cfg.tz;
    int dst = 0;

    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        dst = 1;

    int total = tz + dst;

    /* Apply offset */
    int hh = h + total;
    int dd = d;
    int mm2 = mo;
    int yy = y;

    while (hh < 0) {
        hh += 24;
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

    while (hh >= 24) {
        hh -= 24;
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

    print_datetime_ampm(yy, mm2, dd, hh, m, s);
}


static void cmd_solar(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    int y, mo, d, h;
    rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

    float lat = g_cfg.latitude_e4 * 1e-4f;
    float lon = g_cfg.longitude_e4 * 1e-4f;

    struct solar_times sol;

    /* Request solar in UTC */
    if (!solar_compute(y, mo, d,
                       lat,
                       lon,
                       0,
                       &sol)) {
        console_puts("SOLAR: UNAVAILABLE\n");
        return;
    }

    /* Convert to LOCAL minutes for display */
    int tz = g_cfg.tz;
    int dst = 0;
    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        dst = 1;

    int total = tz + dst;
    int offset_min = total * 60;

    sol.sunrise_std += offset_min;
    sol.sunset_std  += offset_min;
    sol.sunrise_civ += offset_min;
    sol.sunset_civ  += offset_min;

    console_puts("           Rise        Set\n");

    console_puts("Actual     ");
    print_hhmm(sol.sunrise_std);
    console_puts("    ");
    print_hhmm(sol.sunset_std);
    console_putc('\n');

    console_puts("Civil      ");
    print_hhmm(sol.sunrise_civ);
    console_puts("    ");
    print_hhmm(sol.sunset_civ);
    console_putc('\n');
}


static void cmd_schedule(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    /* ------------------------------------------------------------------
     * Read UTC from RTC
     * ------------------------------------------------------------------ */
    int y, mo, d, h, m, s;
    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    /* Convert UTC → LOCAL */
    int tz = g_cfg.tz;
    int dst = 0;

    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        dst = 1;

    int total = tz + dst;

    int ly = y;
    int lmo = mo;
    int ld = d;
    int lh = h + total;

    while (lh < 0) {
        lh += 24;
        ld--;
        if (ld < 1) {
            lmo--;
            if (lmo < 1) {
                lmo = 12;
                ly--;
            }
            ld = days_in_month(ly, lmo);
        }
    }

    while (lh >= 24) {
        lh -= 24;
        ld++;
        if (ld > days_in_month(ly, lmo)) {
            ld = 1;
            lmo++;
            if (lmo > 12) {
                lmo = 1;
                ly++;
            }
        }
    }

    /* ------------------------------------------------------------------
     * Header (LOCAL date)
     * ------------------------------------------------------------------ */
    mini_printf("Today: %04d-%02d-%02d\n\n", ly, lmo, ld);

    mini_printf("lat/long  : %L, %L\n",
                g_cfg.latitude_e4,
                g_cfg.longitude_e4);

    mini_printf("TZ        : %d (DST %s)\n\n",
                g_cfg.tz,
                g_cfg.honor_dst ? "ON" : "OFF");

    /* ------------------------------------------------------------------
     * Solar (compute UTC, print LOCAL)
     * ------------------------------------------------------------------ */

    struct solar_times sol;
    bool have_sol = compute_today_solar(&sol);

    if (have_sol) {

        int offset_min = total * 60;

        int sr = sol.sunrise_std + offset_min;
        int ss2 = sol.sunset_std + offset_min;
        int cr = sol.sunrise_civ + offset_min;
        int cs = sol.sunset_civ + offset_min;

        while (sr < 0) sr += 1440;
        while (sr >= 1440) sr -= 1440;
        while (ss2 < 0) ss2 += 1440;
        while (ss2 >= 1440) ss2 -= 1440;
        while (cr < 0) cr += 1440;
        while (cr >= 1440) cr -= 1440;
        while (cs < 0) cs += 1440;
        while (cs >= 1440) cs -= 1440;

        console_puts("Solar      Rise        Set\n");

        console_puts("Actual     ");
        print_hhmm(sr);
        console_puts("    ");
        print_hhmm(ss2);
        console_putc('\n');

        console_puts("Civil      ");
        print_hhmm(cr);
        console_puts("    ");
        print_hhmm(cs);
        console_putc('\n');
    }
    else {
        console_puts("Solar: UNAVAILABLE\n");
    }

    console_putc('\n');
    console_puts("Events:\n");

    /* ------------------------------------------------------------------
     * Events (resolve UTC, print LOCAL)
     * ------------------------------------------------------------------ */

    size_t used = 0;
    const Event *events = config_events_get(&used);

    if (used == 0) {
        console_puts("(no events)\n");
        return;
    }

    struct Row {
        uint16_t minute;
        const Event *ev;
    };

    struct Row rows[MAX_EVENTS];
    size_t rc = 0;

    for (size_t i = 0; i < MAX_EVENTS; i++) {

        const Event *ev = &events[i];
        if (ev->refnum == 0)
            continue;

        uint16_t minute;
        if (!resolve_when(&ev->when,
                          have_sol ? &sol : NULL,
                          &minute))
            continue;

        rows[rc].minute = minute; /* UTC */
        rows[rc].ev     = ev;
        rc++;
    }

    if (rc == 0) {
        console_puts("(no resolvable events)\n");
        return;
    }

    /* Sort UTC */
    for (size_t i = 0; i + 1 < rc; i++) {
        for (size_t j = i + 1; j < rc; j++) {
            if (rows[j].minute < rows[i].minute) {
                struct Row t = rows[i];
                rows[i] = rows[j];
                rows[j] = t;
            }
        }
    }

    int offset_min = total * 60;

    for (size_t i = 0; i < rc; i++) {

        const Event *ev = rows[i].ev;
        int local_min = (int)rows[i].minute + offset_min;

        while (local_min < 0)
            local_min += 1440;

        while (local_min >= 1440)
            local_min -= 1440;

        const char *dev = "?";
        const char *state = "?";

        device_name(ev->device_id, &dev);

        dev_state_t st =
            (ev->action == ACTION_ON) ? DEV_STATE_ON : DEV_STATE_OFF;

        device_get_state_string(ev->device_id, st, &state);

        mini_printf("%02u:%02u  ",
            (unsigned)(local_min / 60),
            (unsigned)(local_min % 60));

        print_padded(dev,   8);
        print_padded(state, 8);

        when_print(&ev->when);
        console_putc('\n');
    }
}

static void cmd_set(int argc, char **argv)
{
    ensure_cfg_loaded();

    if (argc < 3) {
        console_puts("?\n");
        return;
    }

    /* --------------------------------------------------
     * set date YYYY-MM-DD
     * Commits immediately to RTC using existing RTC time
     * -------------------------------------------------- */
    if (!strcmp(argv[1], "date") && argc == 3) {
        int yy, mm, dd;
        int h, m, s;

        if (!parse_date_ymd(argv[2], &yy, &mm, &dd)) {
            console_puts("ERROR\n");
            return;
        }

        // /* RTC must already have a valid time */
        // if (!rtc_time_is_set()) {
        //     console_puts("ERROR: RTC TIME NOT SET\n");
        //     return;
        // }

        /* Preserve existing RTC time-of-day */
        rtc_get_time(NULL, NULL, NULL, &h, &m, &s);

        if (!rtc_set_time(yy, mm, dd, h, m, s)) {
            console_puts("ERROR: RTC SET FAILED\n");
            return;
        }

        scheduler_invalidate_solar();
        console_puts("OK\n");
        return;
    }

    /* --------------------------------------------------
     * set time HH:MM[:SS]
     * Commits immediately to RTC using existing RTC date
     * Also records epoch at time of set (UTC-normalized)
     * -------------------------------------------------- */
     if (!strcmp(argv[1], "time") && argc == 3) {

         int hh = 0, mi = 0, ss = 0;
         int y, mo, d;

         if (parse_time_hms(argv[2], &hh, &mi, &ss)) {
         } else if (parse_time_hm(argv[2], &hh, &mi)) {
             ss = 0;
         } else {
             console_puts("ERROR\n");
             return;
         }

         if (!rtc_time_is_set()) {
             console_puts("ERROR: RTC DATE NOT SET\n");
             return;
         }

         /* Existing date from RTC (UTC date) */
         rtc_get_time(&y, &mo, &d, NULL, NULL, NULL);

         /*
          * User entered LOCAL time.
          * Convert LOCAL → UTC before programming RTC.
          */
         int tz = g_cfg.tz;
         int dst = 0;

         if (g_cfg.honor_dst && is_us_dst(y, mo, d, hh))
             dst = 1;

         int total = tz + dst;

         int utc_h = hh - total;

         while (utc_h < 0) {
             utc_h += 24;
             d--;
         }

         while (utc_h >= 24) {
             utc_h -= 24;
             d++;
         }

         if (!rtc_set_time(y, mo, d, utc_h, mi, ss)) {
             console_puts("ERROR: RTC SET FAILED\n");
             return;
         }

         /* Drift baseline is pure UTC */
         g_cfg.rtc_set_epoch =
             rtc_epoch_from_ymdhms(
                 y, mo, d,
                 utc_h, mi, ss,
                 0,
                 false
             );

         config_save(&g_cfg);
         g_cfg_dirty = false;

         console_puts("OK\n");
         return;
     }

    /* --------------------------------------------------
     * set lat +/-DD.DDDD
     * -------------------------------------------------- */
    if (!strcmp(argv[1], "lat") && argc == 3) {
        float v = atof(argv[2]);
        if (v < -90.0f || v > 90.0f) {
            console_puts("ERROR\n");
            return;
        }

        g_cfg.latitude_e4 = (int32_t)(v * 10000.0f);
        g_cfg_dirty = true;
        scheduler_invalidate_solar();
        console_puts("OK\n");
        return;
    }

    /* --------------------------------------------------
     * set lon +/-DDD.DDDD
     * -------------------------------------------------- */
    if (!strcmp(argv[1], "lon") && argc == 3) {
        float v = atof(argv[2]);
        if (v < -180.0f || v > 180.0f) {
            console_puts("ERROR\n");
            return;
        }

        g_cfg.longitude_e4 = (int32_t)(v * 10000.0f);
        g_cfg_dirty = true;
        scheduler_invalidate_solar();
        console_puts("OK\n");
        return;
    }

    /* --------------------------------------------------
     * set tz +/-HH
     * -------------------------------------------------- */
    if (!strcmp(argv[1], "tz") && argc == 3) {
        int v = atoi(argv[2]);
        if (v < -12 || v > 14) {
            console_puts("ERROR\n");
            return;
        }

        g_cfg.tz = v;
        g_cfg_dirty = true;
        scheduler_invalidate_solar();
        console_puts("OK\n");
        return;
    }

    /* --------------------------------------------------
     * set dst on|off
     * -------------------------------------------------- */
    if (!strcmp(argv[1], "dst") && argc == 3) {
        if (!strcmp(argv[2], "on"))
            g_cfg.honor_dst = true;
        else if (!strcmp(argv[2], "off"))
            g_cfg.honor_dst = false;
        else {
            console_puts("ERROR\n");
            return;
        }

        g_cfg_dirty = true;
        scheduler_invalidate_solar();
        console_puts("OK\n");
        return;
    }

    /* --------------------------------------------------
     * Mechanical timing parameters (unchanged)
     * -------------------------------------------------- */

    if (!strcmp(argv[1], "lock_pulse_ms") && argc == 3) {
        int v = atoi(argv[2]);
        if (v < 50 || v > 5001) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.lock_pulse_ms = (uint16_t)v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    if (!strcmp(argv[1], "door_settle_ms") && argc == 3) {
        int v = atoi(argv[2]);
        if (v < 50 || v > 5001) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.door_settle_ms = (uint16_t)v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    if (!strcmp(argv[1], "lock_settle_ms") && argc == 3) {
        int v = atoi(argv[2]);
        if (v > 2001) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.lock_settle_ms = (uint16_t)v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    if (!strcmp(argv[1], "door_travel_ms") && argc == 3) {
        int v = atoi(argv[2]);
        if (v < 1000 || v > 30000) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.door_travel_ms = (uint16_t)v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    console_puts("?\n");
}

 static void cmd_device(int argc, char **argv)
 {
     /* device */
     if (argc == 1) {
            uint8_t id;

            for (bool ok = device_enum_first(&id);
                ok;
                ok = device_enum_next(id, &id)) {

                dev_state_t st;
                const char *name;
                const char *str;

                if(id == DEVICE_ID_LED)
                    continue;

                if (!device_get_state_by_id(id, &st))
                    continue;

                if (!device_name(id, &name))
                    continue;

                if (!device_get_state_string(id, st, &str))
                    continue;

                console_puts(name);
                console_puts(": ");
                console_puts(str);
                console_putc('\n');
            }

         return;
     }

     /* device <name> <state> */
     if (argc == 3) {
         uint8_t id;

         /* NOTE: 0 == fail */
         if (!device_lookup_id(argv[1], &id)) {
             console_puts("ERROR\n");
             return;
         }

         dev_state_t want;
         if (!device_parse_state_by_id(id, argv[2], &want)) {
             console_puts("ERROR\n");
             return;
         }

         if (!device_set_state_by_id(id,want)) {
             console_puts("ERROR\n");
             return;
         }

         console_puts("OK\n");
         return;
     }

     console_puts("?\n");
 }


 static void cmd_save(int argc, char **argv)
 {
     (void)argc;
     (void)argv;

     ensure_cfg_loaded();

     config_save(&g_cfg);

     g_cfg_dirty = false;
     console_puts("OK\n");
 }

 static void cmd_door(int argc, char **argv)
 {
     if (argc != 2) {
         console_puts("usage: door open|close|toggle|status\n");
         return;
     }

     if (!strcmp(argv[1], "open")) {
         door_sm_request(DEV_STATE_ON);
     }
     else if (!strcmp(argv[1], "close")) {
         door_sm_request(DEV_STATE_OFF);
     }
     else if (!strcmp(argv[1], "toggle")) {
         door_sm_toggle();
     }
     else if (!strcmp(argv[1], "status")) {
         /* no action */
     }
     else {
         console_puts("?\n");
         return;
     }

     mini_printf("door: %s  motion=%s\n",
                 door_sm_state_string(),
                 door_sm_motion_string());
 }



// src/console/console_cmds.cpp

static void cmd_lock(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: lock engage|release\n");
        return;
    }

    if (!strcmp(argv[1], "engage")) {
        console_puts("Locking...\n");
        door_lock_engage();     /* blocking, safe */
        console_puts("Lock engaged\n");
        return;
    }

    if (!strcmp(argv[1], "release")) {
        console_puts("Unlocking...\n");
        door_lock_release();    /* blocking, safe */
        console_puts("Lock released\n");
        return;
    }

    console_puts("usage: lock engage|release\n");
}

static void cmd_led(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: led off|red|green|pulse_red|pulse_green|blink_red|blink_green\n");
        return;
    }

    const char *s = argv[1];

    if (!strcmp(s, "off"))               led_state_machine_set(LED_OFF,LED_RED );
    else if (!strcmp(s, "red"))         led_state_machine_set(LED_ON, LED_RED);
    else if (!strcmp(s, "green"))       led_state_machine_set(LED_ON, LED_GREEN);
    else if (!strcmp(s, "pulse_red"))   led_state_machine_set(LED_PULSE, LED_RED);
    else if (!strcmp(s, "pulse_green")) led_state_machine_set(LED_PULSE, LED_GREEN);
    else if (!strcmp(s, "blink_red"))   led_state_machine_set(LED_BLINK, LED_RED);
    else if (!strcmp(s, "blink_green")) led_state_machine_set(LED_BLINK, LED_GREEN);
    else {
        console_puts("ERROR\n");
        return;
    }

    console_puts("OK\n");
}


static void cmd_rtc(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!rtc_time_is_set()) {
        console_puts("RTC: INVALID (oscillator stopped or time not set)\n");
        return;
    }

    int y, mo, d, h, m, s;
    rtc_get_time(&y, &mo, &d, &h, &m, &s);   /* UTC */

    uint32_t epoch = rtc_get_epoch();        /* UTC epoch */

    console_puts("RTC: VALID\n");

    /* --------------------------------------------------
     * Raw UTC (hardware truth)
     * -------------------------------------------------- */
    mini_printf("utc date : %04d-%02d-%02d\n", y, mo, d);
    mini_printf("utc time : %02d:%02d:%02d\n", h, m, s);
    mini_printf("epoch    : %lu\n", (unsigned long)epoch);

    uint16_t mins = rtc_minutes_since_midnight();
    mini_printf("utc minute_of_day: %u\n", (unsigned)mins);

    /* --------------------------------------------------
     * Derived LOCAL time (presentation only)
     * -------------------------------------------------- */

    int tz = g_cfg.tz;
    int dst = 0;

    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        dst = 1;

    int total = tz + dst;

    int ly = y;
    int lmo = mo;
    int ld = d;
    int lh = h + total;

    while (lh < 0) {
        lh += 24;
        ld--;
        if (ld < 1) {
            lmo--;
            if (lmo < 1) {
                lmo = 12;
                ly--;
            }
            ld = days_in_month(ly, lmo);
        }
    }

    while (lh >= 24) {
        lh -= 24;
        ld++;
        if (ld > days_in_month(ly, lmo)) {
            ld = 1;
            lmo++;
            if (lmo > 12) {
                lmo = 1;
                ly++;
            }
        }
    }

    mini_printf("local date: %04d-%02d-%02d\n", ly, lmo, ld);
    mini_printf("local time: %02d:%02d:%02d\n", lh, m, s);

    /* --------------------------------------------------
     * Drift measurement (UTC basis only)
     * -------------------------------------------------- */

    if (g_cfg.rtc_set_epoch == 0) {
        console_puts("since_set: UNKNOWN\n");
        return;
    }

    uint32_t delta = 0;

    if (epoch >= g_cfg.rtc_set_epoch)
        delta = epoch - g_cfg.rtc_set_epoch;

    uint32_t days  = delta / 86400u;
    delta %= 86400u;

    uint32_t hours = delta / 3600u;
    delta %= 3600u;

    uint32_t mins2 = delta / 60u;
    uint32_t secs  = delta % 60u;

    mini_printf(
        "since_set: %lu sec (%lu days %02lu:%02lu:%02lu)\n",
        (unsigned long)(epoch - g_cfg.rtc_set_epoch),
        (unsigned long)days,
        (unsigned long)hours,
        (unsigned long)mins2,
        (unsigned long)secs
    );
}

static void cmd_config(int, char **)
{
    ensure_cfg_loaded();

    if (g_cfg_dirty)
        console_puts("CONFIG (UNSAVED)\n\n");
    else
        console_puts("CONFIG (SAVED)\n\n");

    /* lat / lon / tz */
    mini_printf("lat  : %L\n", g_cfg.latitude_e4);
    mini_printf("lon  : %L\n", g_cfg.longitude_e4);
    mini_printf("tz   : %d\n",   g_cfg.tz);

    mini_printf("dst  : %s\n",
                g_cfg.honor_dst ? "ON (US rules)" : "OFF");

    /* drift baseline */
    if (g_cfg.rtc_set_epoch != 0) {
        mini_printf("rtc_set_epoch : %lu\n",
                    (unsigned long)g_cfg.rtc_set_epoch);
    } else {
        console_puts("rtc_set_epoch : (not set)\n");
    }


    /* mechanical timing */
     mini_printf("door_travel_ms : %u\n", g_cfg.door_travel_ms);
     mini_printf("door_settle_ms : %u\n", g_cfg.door_settle_ms);
     mini_printf("lock_pulse_ms  : %u\n", g_cfg.lock_pulse_ms);
     mini_printf("lock_settle_ms : %u\n", g_cfg.lock_settle_ms);

    console_putc('\n');
}



void console_help(int argc, char **argv);

static void cmd_event(int argc, char **argv)
{
    ensure_cfg_loaded();

    /* Default: `event` == `event list` */
    if (argc == 1) {
        argv[1] = (char *)"list";
        argc = 2;
    }

    /* --------------------------------------------------------------------
     * event list
     * ------------------------------------------------------------------ */
    if (!strcmp(argv[1], "list") && argc == 2) {

        size_t count = 0;
        const Event *events = config_events_get(&count);

        if (count == 0) {
            console_puts("(no events)\n");
            return;
        }

        /* Compute solar times once for today */
        struct solar_times sol;
        bool have_sol = compute_today_solar(&sol);

        /* Collect resolved events (by refnum, not index) */
        struct Resolved {
            uint16_t minute;
            refnum_t refnum;
            const Event *ev;
        };

        struct Resolved r[MAX_EVENTS];
        size_t rcount = 0;

        /* IMPORTANT:
         * config_events_get() returns the full sparse table.
         * We must scan MAX_EVENTS and skip refnum == 0.
         */
        for (size_t i = 0; i < MAX_EVENTS; i++) {
            const Event *ev = &events[i];

            if (ev->refnum == 0)
                continue;

            uint16_t minute;
            if (!resolve_when(&ev->when,
                              have_sol ? &sol : NULL,
                              &minute))
                continue;

            r[rcount].minute = minute;
            r[rcount].refnum = ev->refnum;
            r[rcount].ev     = ev;
            rcount++;

            if (rcount >= MAX_EVENTS)
                   break;
        }

        if (rcount == 0) {
            console_puts("(no events)\n");
            return;
        }

        /* Sort by resolved time, then refnum (stable) */
        for (size_t i = 0; i + 1 < rcount; i++) {
            for (size_t j = i + 1; j < rcount; j++) {
                if (r[j].minute < r[i].minute ||
                   (r[j].minute == r[i].minute &&
                    r[j].refnum < r[i].refnum)) {

                    struct Resolved tmp = r[i];
                    r[i] = r[j];
                    r[j] = tmp;
                }
            }
        }

        /* Print */
        for (size_t i = 0; i < rcount; i++) {

            const Event *ev = r[i].ev;
            uint16_t utc_minute = r[i].minute;

            /* ---- Convert UTC minute → LOCAL minute ---- */

            int tz = g_cfg.tz;
            int dst = 0;

            int y, mo, d, h;
            rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

            if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
                dst = 1;

            int total = tz + dst;
            int offset_min = total * 60;

            int local_minute = (int)utc_minute + offset_min;

            while (local_minute < 0)
                local_minute += 1440;

            while (local_minute >= 1440)
                local_minute -= 1440;

            /* ---- Device/state ---- */

            const char *dev_name = "?";
            const char *state    = "?";

            device_name(ev->device_id, &dev_name);

            dev_state_t st =
                (ev->action == ACTION_ON) ? DEV_STATE_ON : DEV_STATE_OFF;

            device_get_state_string(ev->device_id, st, &state);

            /* ---- Print LOCAL time ---- */

            mini_printf("%02u:%02u  #",
                        (unsigned)(local_minute / 60),
                        (unsigned)(local_minute % 60));

            print_uint_padded(ev->refnum, 3);
            console_puts("  ");

            print_padded(dev_name, 8);
            console_putc(' ');
            print_padded(state, 7);
            console_putc(' ');

            when_print(&ev->when);
            console_putc('\n');
        }

        return;
    }

    /* --------------------------------------------------------------------
     * event clear
     * ------------------------------------------------------------------ */
    if (!strcmp(argv[1], "clear") && argc == 2) {
        config_events_clear();
        g_cfg_dirty = true;
        console_puts("OK (events cleared, not saved)\n");
        return;
    }

    /* --------------------------------------------------------------------
     * event delete <refnum>
     * ------------------------------------------------------------------ */
    if (!strcmp(argv[1], "delete") && argc == 3) {

        char *end = NULL;
        long ref = strtol(argv[2], &end, 10);

        if (!end || *end != '\0' || ref <= 0 || ref > 255) {
            console_puts("ERROR\n");
            return;
        }

        if (!config_events_delete_by_refnum((refnum_t)ref)) {
            console_puts("ERROR\n");
            return;
        }

        g_cfg_dirty = true;
        console_puts("OK (event deleted, not saved)\n");
        return;
    }

    /* --------------------------------------------------------------------
      * event add ...
      * ------------------------------------------------------------------ */
     if (!strcmp(argv[1], "add")) {

         Event ev;
         memset(&ev, 0, sizeof(ev));

         if (argc < 5) {
             console_puts("ERROR ARGS\n");
             return;
         }

         /* --------------------------------------------------
          * Device
          * -------------------------------------------------- */
         if (!device_lookup_id(argv[2], &ev.device_id)) {
             console_puts("ERROR DEVICE\n");
             return;
         }

         /* --------------------------------------------------
          * State
          * -------------------------------------------------- */
         dev_state_t st;
         if (!device_parse_state_by_id(ev.device_id, argv[3], &st)) {
             console_puts("ERROR STATE\n");
             return;
         }

         if (st == DEV_STATE_ON)
             ev.action = ACTION_ON;
         else if (st == DEV_STATE_OFF)
             ev.action = ACTION_OFF;
         else {
             console_puts("ERROR STATE\n");
             return;
         }

         /* --------------------------------------------------
          * WHEN parsing
          * -------------------------------------------------- */

         /* implicit HH:MM */
         if (argc == 5) {
             int hh, mm;
             if (parse_time_hm(argv[4], &hh, &mm)) {

                 /* Convert LOCAL → UTC */
                 int tz = g_cfg.tz;
                 int dst = 0;

                 int y, mo, d, h;
                 rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

                 if (g_cfg.honor_dst && is_us_dst(y, mo, d, hh))
                     dst = 1;

                 int total = tz + dst;

                 int utc_h = hh - total;
                 int utc_min = utc_h * 60 + mm;

                 while (utc_min < 0)
                     utc_min += 1440;

                 while (utc_min >= 1440)
                     utc_min -= 1440;

                 ev.when.ref = REF_MIDNIGHT;
                 ev.when.offset_minutes = (int16_t)utc_min;

                 goto add_event;
             }
         }

         /* explicit midnight HH:MM */
         if (argc == 6 && !strcmp(argv[4], "midnight")) {
             int hh, mm;
             if (!parse_time_hm(argv[5], &hh, &mm)) {
                 console_puts("ERROR TIME\n");
                 return;
             }

             int tz = g_cfg.tz;
             int dst = 0;

             int y, mo, d, h;
             rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

             if (g_cfg.honor_dst && is_us_dst(y, mo, d, hh))
                 dst = 1;

             int total = tz + dst;

             int utc_h = hh - total;
             int utc_min = utc_h * 60 + mm;

             while (utc_min < 0)
                 utc_min += 1440;

             while (utc_min >= 1440)
                 utc_min -= 1440;

             ev.when.ref = REF_MIDNIGHT;
             ev.when.offset_minutes = (int16_t)utc_min;

             goto add_event;
         }

         /* solar / civil anchors */
         static const struct {
             const char *name;
             decltype(ev.when.ref) ref;
         } when_keywords[] = {
             { "sunrise", REF_SOLAR_STD_RISE },
             { "sunset",  REF_SOLAR_STD_SET  },
             { "dawn",    REF_SOLAR_CIV_RISE },
             { "dusk",    REF_SOLAR_CIV_SET  },
         };

         for (size_t i = 0; i < sizeof(when_keywords)/sizeof(when_keywords[0]); i++) {
             if (!strcmp(argv[4], when_keywords[i].name)) {

                 ev.when.ref = when_keywords[i].ref;
                 ev.when.offset_minutes = 0;

                 /* optional offset */
                 if (argc == 6) {
                     int off;
                     if (!parse_signed_int(argv[5], &off)) {
                         console_puts("ERROR OFFSET\n");
                         return;
                     }
                     ev.when.offset_minutes = (int16_t)off;
                 }

                 goto add_event;
             }
         }

         console_puts("ERROR FORMAT\n");
         return;

     add_event:
         ev.refnum = 0;

         if (!config_events_add(&ev)) {
             console_puts("ERROR\n");
             return;
         }

         g_cfg_dirty = true;
         console_puts("OK (event added, not saved)\n");
         return;
     }

    console_puts("?\n");
}



static void cmd_sleep(int argc, char **argv)
{
    ensure_cfg_loaded();

    if (argc < 2) {
        console_puts("usage: sleep <minutes|next>\n");
        return;
    }

    if (!rtc_time_is_set()) {
        console_puts("sleep: RTC not set\n");
        return;
    }

    uint16_t target = 0;

    /* ==========================================================
     * sleep next
     * ========================================================== */
    if (!strcmp(argv[1], "next")) {

        uint16_t now_min = rtc_minutes_since_midnight();

        uint16_t next_min;
        if (!scheduler_next_event_minute(&next_min)) {
            console_puts("sleep: no scheduled events\n");
            return;
        }

        if (next_min <= now_min)
            next_min = (uint16_t)((now_min + 1u) % 1440u);

        target = next_min;

        mini_printf("sleep: until %02u:%02u\n",
                    (unsigned)(target / 60u),
                    (unsigned)(target % 60u));
    }
    else
    {
        /* ==========================================================
         * sleep <minutes>
         * ========================================================== */

        int minutes = atoi(argv[1]);
        if (minutes <= 0 || minutes > 1440) {
            console_puts("sleep: invalid minutes\n");
            return;
        }

        int y, mo, d, h, m, s;
        rtc_get_time(&y, &mo, &d, &h, &m, &s);

        if (s > 0) {
            m++;
            if (m >= 60) {
                m = 0;
                h = (h + 1) % 24;
            }
        }

        uint16_t now_min = (uint16_t)(h * 60u + m);
        target = (uint16_t)((now_min + (uint16_t)minutes) % 1440u);

        if (target <= now_min)
            target = (uint16_t)((now_min + 1u) % 1440u);

        mini_printf("sleep: %u minute(s)\n", (unsigned)minutes);
        mini_printf("now    : %02u:%02u\n",
                    (unsigned)(now_min / 60u),
                    (unsigned)(now_min % 60u));
        mini_printf("target : %02u:%02u\n",
                    (unsigned)(target / 60u),
                    (unsigned)(target % 60u));
    }

    /* ----------------------------------------------------------
     * Program alarm
     * ---------------------------------------------------------- */

    rtc_alarm_disable();
    rtc_alarm_clear_flag();

    if (!rtc_alarm_set_minute_of_day(target)) {
        console_puts("sleep: alarm set failed\n");
        return;
    }

    /* ----------------------------------------------------------
     * Sleep
     * ---------------------------------------------------------- */

    system_sleep_until(target);

    /* ----------------------------------------------------------
     * WAKE ANALYSIS
     * ---------------------------------------------------------- */

    bool woke_rtc  = gpio_rtc_int_is_asserted();
    bool woke_door = gpio_door_sw_is_asserted();

    /* If RTC woke us, clear AF */
    if (woke_rtc) {
        rtc_alarm_clear_flag();
    }

    /* If door woke us, wait for release (bounded) */
    if (woke_door) {
        for (uint16_t i = 0; i < 5000u; i++) {
            if (!gpio_door_sw_is_asserted())
                break;
            _delay_ms(1);
        }
    }

    /* Clear interrupt flags */
    EIFR |= (1u << INTF0) | (1u << INTF1);

    /* Re-arm interrupts only if lines are HIGH */
    if (!gpio_rtc_int_is_asserted())
        EIMSK |= (1u << INT0);

    if (!gpio_door_sw_is_asserted())
        EIMSK |= (1u << INT1);

    /* Visual feedback */
    if (woke_door)
        led_state_machine_set(LED_BLINK, LED_RED, 3);
    else if (woke_rtc)
        led_state_machine_set(LED_BLINK, LED_GREEN, 3);

    mini_printf("woke: rtc=%u door=%u\n",
                woke_rtc ? 1u : 0u,
                woke_door ? 1u : 0u);
}


typedef void (*cmd_fn_t)(int argc, char **argv);

typedef struct {
    const char *cmd;
    uint8_t     min_args;
    uint8_t     max_args;
    cmd_fn_t    handler;
    const char *help_short;
    const char *help_long;
} cmd_entry_t;



/// -------------------------
/*
 * Canonical command list
 * name, min, max, handler, short_help, long_help
 */
#define CMD_LIST(X) \
    X(help, 0, 1, console_help, \
      "Show help", \
      "help\n" \
      "help <command>\n" \
      "  Show top-level command list or detailed help for a command\n" \
    ) \
    \
    X(version, 0, 0, cmd_version, \
      "Show firmware version", \
      "version\n" \
      "  Show firmware version and build date\n" \
    ) \
    \
    X(time, 0, 0, cmd_time, \
      "Show current date/time", \
      "time\n" \
      "  Show RTC date and time\n" \
      "  Format: YYYY-MM-DD HH:MM:SS AM|PM\n" \
    ) \
    \
    X(schedule, 0, 0, cmd_schedule, \
      "Show schedule", \
      "schedule\n" \
      "  Show system schedule and next resolved events\n" \
    ) \
    \
    X(solar, 0, 0, cmd_solar, \
      "Show sunrise/sunset times", \
      "solar\n" \
      "  Show stored location and today's solar times\n" \
    ) \
    \
    X(set, 2, 6, cmd_set, \
      "Configure settings", \
      "set date YYYY-MM-DD\n" \
      "set time HH:MM:SS\n" \
      "set lat  +/-DD.DDDD\n" \
      "set lon  +/-DDD.DDDD\n" \
      "set tz   +/-HH\n" \
    ) \
    \
    X(config, 0, 0, cmd_config, \
      "Show configuration", \
      "config\n" \
      "  Show current configuration values\n" \
      "  Note: changes are not committed until save\n" \
    ) \
    \
    X(save, 0, 0, cmd_save, \
      "Commit settings", \
      "save\n" \
      "  Commit configuration to EEPROM and program RTC\n" \
    ) \
    \
    X(device, 0, 3, cmd_device, \
      "Show or set device state", \
      "device\n" \
      "device <name>\n" \
      "device <name> on|off\n" \
      "  Show all device states, show one device, or set device state\n" \
    ) \
    \
    X(door, 1, 2, cmd_door, \
      "Manually control door", \
      "door open\n" \
      "door close\n" \
      "  Manually actuate the coop door\n" \
    ) \
    \
    X(lock, 1, 2, cmd_lock, \
      "Manually control lock", \
      "lock engage\n" \
      "lock release\n" \
      "  Manually engage or release the door lock\n" \
    ) \
    \
    X(event, 0, 7, cmd_event, \
      "Event commands", \
      "event list\n" \
      "event add <device> <on|off> HH:MM\n" \
      "event add <device> <on|off> midnight HH:MM\n" \
      "event add <device> <on|off> sunrise +/-MIN\n" \
      "event add <device> <on|off> sunset  +/-MIN\n" \
      "event add <device> <on|off> dawn    +/-MIN\n" \
      "event add <device> <on|off> dusk    +/-MIN\n" \
      "event delete <refnum>\n" \
    ) \
    \
    X(led, 1, 1, cmd_led, \
      "Control door LED", \
      "led off\n" \
      "led red\n" \
      "led green\n" \
      "led pulse_red\n" \
      "led pulse_green\n" \
      "led blink_red\n" \
      "led blink_green\n" \
    ) \
    \
    X(rtc, 0, 0, cmd_rtc, \
      "Show raw RTC state", \
      "rtc\n" \
      "  Display raw RTC date/time and validity\n" \
      "  No DST, no staging, no scheduler logic\n" \
    ) \
    X(sleep, 0, 1, cmd_sleep, \
          "Sleep til next scheduled event", \
          "sleep\n" \
          "sleep <minutes>\n" \
          "  sleep till the next resolved scheduler event (if any)\n" \
    )



/* ------------------------------------------------------------
 * Command string declarations
 * ------------------------------------------------------------ */
 /* ------------------------------------------------------------
  * Command string declarations (flash)
  * ------------------------------------------------------------ */

 #include <avr/pgmspace.h>

 #define DECLARE_CMD_STRINGS(name, min, max, fn, short_h, long_h) \
     static const char cmd_##name##_name[]  PROGMEM = #name; \
     static const char cmd_##name##_short[] PROGMEM = short_h; \
     static const char cmd_##name##_long[]  PROGMEM = long_h;

 CMD_LIST(DECLARE_CMD_STRINGS)

 #undef DECLARE_CMD_STRINGS


 /* ------------------------------------------------------------
  * Command table (flash)
  * ------------------------------------------------------------ */

 #define MAKE_CMD_ENTRY(name, min, max, fn, short_h, long_h) \
     { cmd_##name##_name, min, max, fn, cmd_##name##_short, cmd_##name##_long },

 static const cmd_entry_t cmd_table[] PROGMEM = {
     CMD_LIST(MAKE_CMD_ENTRY)
 };

 #undef MAKE_CMD_ENTRY

 #define CMD_TABLE_LEN (sizeof(cmd_table) / sizeof(cmd_table[0]))
static void read_cmd_entry(cmd_entry_t *dst, unsigned idx)
{
    memcpy_P(dst, &cmd_table[idx], sizeof(cmd_entry_t));
}

void console_help(int argc, char **argv)
{
    cmd_entry_t e;

    /* help */
    if (argc == 1) {
        console_puts("Commands:\n");

        unsigned max_len = 0;
        for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
            read_cmd_entry(&e, i);
            unsigned len = console_strlen(e.cmd);
            if (len > max_len)
                max_len = len;
        }

        for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
            read_cmd_entry(&e, i);

            console_puts("  ");
            console_puts_str(e.cmd);

            unsigned len = console_strlen(e.cmd);
            while (len++ < max_len + 2)
                console_putc(' ');

            console_puts_str(e.help_short);
            console_putc('\n');
        }

        console_puts("\nType: help <command>\n");
        return;
    }

    /* help <command> */
    for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
        read_cmd_entry(&e, i);

        if (console_strcmp(argv[1], e.cmd) == 0) {
            if (e.help_long)
                console_puts_str(e.help_long);
            return;
        }
    }

    console_puts("?\n");
}

void console_dispatch(int argc, char **argv)
{
    if (argc == 0)
        return;

    str_to_lower(argv[0]);

    cmd_entry_t e;

    for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
        read_cmd_entry(&e, i);

        if (console_strcmp(argv[0], e.cmd) == 0) {

            int args = argc - 1;

            if (args < e.min_args || args > e.max_args) {
                if (e.help_short) {
                    console_puts_str(e.help_short);
                    console_putc('\n');
                }
                return;
            }

            e.handler(argc, argv);
            return;
        }
    }

    console_puts("?\n");
}
