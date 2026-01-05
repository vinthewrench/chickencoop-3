
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
 *  - On host builds, the same source compiles to normal C strings for
 *    simplicity and debuggability.
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


#include "console/console_io.h"
#include "console/console.h"
#include "console/mini_printf.h"
#include "time_dst.h"
#include "console_time.h"
#include "door_led.h"

#include "events.h"
#include "config_events.h"
#include "resolve_when.h"
#include "next_event.h"

#include "solar.h"
#include "rtc.h"
#include "config.h"
#include "lock.h"
#include "door.h"
#include "relay.h"

#include "uptime.h"

#include "devices/devices.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

extern bool want_exit;

// -----------------------------------------------------------------------------
// CONFIG shadow state
//   - set commands modify RAM shadow only
//   - save commits to EEPROM and programs RTC
// -----------------------------------------------------------------------------

static bool g_cfg_loaded = false;
static bool g_cfg_dirty = false;

static int g_date_y = 0, g_date_mo = 0, g_date_d = 0;
static int g_time_h = 0, g_time_m = 0, g_time_s = 0;
static bool g_have_date = false;
static bool g_have_time = false;
static uint32_t g_time_set_uptime_s = 0;


/* Command handler forward declarations */
static void console_help(int argc, char **argv);
static void cmd_version(int argc, char **argv);
static void cmd_time(int argc, char **argv);
static void cmd_schedule(int argc, char **argv);
static void cmd_solar(int argc, char **argv);
static void cmd_set(int argc, char **argv);
static void cmd_config(int argc, char **argv);
static void cmd_save(int argc, char **argv);
static void cmd_timeout(int argc, char **argv);
static void cmd_door(int argc, char **argv);
static void cmd_lock(int argc, char **argv);
static void cmd_run(int argc, char **argv);
static void cmd_event(int argc, char **argv);
static void cmd_next(int argc, char **argv);

#ifdef HOST_BUILD
static void cmd_sleep(int argc, char **argv);
#endif
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

    // Seed shadow date/time from RTC if it is already valid.
    if (rtc_time_is_set()) {
        rtc_get_time(&g_date_y, &g_date_mo, &g_date_d, &g_time_h, &g_time_m, &g_time_s);
        g_have_date = true;
        g_have_time = true;
        g_time_set_uptime_s = uptime_seconds();
    }
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

static void advance_one_day(int *y, int *mo, int *d)
{
    int dim = days_in_month(*y, *mo);
    (*d)++;
    if (*d <= dim) return;

    *d = 1;
    (*mo)++;
    if (*mo <= 12) return;

    *mo = 1;
    (*y)++;
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


static void when_print(const struct When *w)
{
    if (!w) {
        console_puts("?");
        return;
    }

    switch (w->ref) {

    case REF_NONE:
        console_puts("DISABLED");
        return;

    case REF_MIDNIGHT: {
        int h = w->offset_minutes / 60;
        int m = abs(w->offset_minutes % 60);
        mini_printf("%02d:%02d", h, m);
        return;
    }

    case REF_SOLAR_STD_RISE:
        mini_printf("SOLAR SUNRISE %c%d",
                    (w->offset_minutes < 0) ? '-' : '+',
                    abs(w->offset_minutes));
        return;

    case REF_SOLAR_STD_SET:
        mini_printf("SOLAR SUNSET %c%d",
                    (w->offset_minutes < 0) ? '-' : '+',
                    abs(w->offset_minutes));
        return;

    case REF_SOLAR_CIV_RISE:
        mini_printf("CIVIL DAWN %c%d",
                    (w->offset_minutes < 0) ? '-' : '+',
                    abs(w->offset_minutes));
        return;

    case REF_SOLAR_CIV_SET:
        mini_printf("CIVIL DUSK %c%d",
                    (w->offset_minutes < 0) ? '-' : '+',
                    abs(w->offset_minutes));
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

    double lat = (double)g_cfg.latitude_e4 / 10000.0;
    double lon = (double)g_cfg.longitude_e4 / 10000.0;

    int tz = g_cfg.tz;
    if (g_cfg.honor_dst &&
        is_us_dst(g_date_y, g_date_mo, g_date_d, g_time_h)) {
        tz += 1;
    }

    return solar_compute(
        g_date_y,
        g_date_mo,
        g_date_d,
        lat,
        lon,
        (int8_t)tz,
        out
    );
}

// -----------------------------------------------------------------------------
// Enum-to-string helpers
// -----------------------------------------------------------------------------

static const char *action_name(enum Action a)
{
    switch (a) {
    case ACTION_ON:  return "on";
    case ACTION_OFF: return "off";
    default:         return "?";
    }
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

    /* If time is staged, show staged time advancing */
    if (g_have_date && g_have_time) {

        y = g_date_y;
        mo = g_date_mo;
        d = g_date_d;
        h = g_time_h;
        m = g_time_m;
        s = g_time_s;

        uint32_t now_s = uptime_seconds();
        uint32_t delta_s = now_s - g_time_set_uptime_s;

        /* Add elapsed seconds (same logic as save) */
        s += (int)(delta_s % 60);
        delta_s /= 60;
        m += (int)(delta_s % 60);
        delta_s /= 60;
        h += (int)(delta_s % 24);
        delta_s /= 24;

        while (s >= 60) { s -= 60; m++; }
        while (m >= 60) { m -= 60; h++; }
        while (h >= 24) { h -= 24; delta_s++; }

        while (delta_s--) {
            advance_one_day(&y, &mo, &d);
        }

        print_datetime_ampm(y, mo, d, h, m, s);
        return;
    }

    /* Otherwise fall back to RTC */
    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    rtc_get_time(&y, &mo, &d, &h, &m, &s);
    print_datetime_ampm(y, mo, d, h, m, s);
}



static void cmd_solar(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    int y, mo, d, h, m, s;
    int ry, rmo, rd;   /* throwaway RTC date */

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    if (g_have_date) {
        /* Console shadow date is authoritative */
        y  = g_date_y;
        mo = g_date_mo;
        d  = g_date_d;

        /* rtc_get_time requires non-NULL pointers */
        rtc_get_time(&ry, &rmo, &rd, &h, &m, &s);
    } else {
        rtc_get_time(&y, &mo, &d, &h, &m, &s);
    }


    int effective_tz = g_cfg.tz;
    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h))
        effective_tz += 1;


    float lat = g_cfg.latitude_e4 * 1e-4f;
    float lon = g_cfg.longitude_e4 * 1e-4f;

    struct solar_times sol;
    if (!solar_compute(y, mo, d,
                       lat,
                       lon,
                       effective_tz,
                       &sol)) {
        console_puts("SOLAR: UNAVAILABLE\n");
        return;
    }

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



static void cmd_timeout(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: timeout on|off\n");
        return;
    }

    if (!strcmp(argv[1], "off")) {
        console_timeout_enabled = false;
        console_puts("TIMEOUT DISABLED\n");
        return;
    }

    if (!strcmp(argv[1], "on")) {
        console_timeout_enabled = true;
        console_puts("TIMEOUT ENABLED\n");
        return;
    }

    console_puts("?\n");
}

static void cmd_schedule(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    int y, mo, d, h, m, s;
    int ry, rmo, rd;   /* throwaway date from RTC */

    if (g_have_date) {
        /* Console shadow date is authoritative */
        y  = g_date_y;
        mo = g_date_mo;
        d  = g_date_d;

        if (!rtc_time_is_set()) {
            console_puts("TIME: NOT SET\n");
            return;
        }

        /* rtc_get_time requires non-NULL pointers */
        rtc_get_time(&ry, &rmo, &rd, &h, &m, &s);
    } else {
        if (!rtc_time_is_set()) {
            console_puts("TIME: NOT SET\n");
            return;
        }
        rtc_get_time(&y, &mo, &d, &h, &m, &s);
    }

    int effective_tz = g_cfg.tz;
    struct solar_times sol;

    /* Match cmd_solar() DST handling */
    if (g_cfg.honor_dst && is_us_dst(y, mo, d, 12)) {
        effective_tz += 1;
    }

    float lat = g_cfg.latitude_e4 * 1e-4f;
    float lon = g_cfg.longitude_e4 * 1e-4f;

    if (!solar_compute(y, mo, d,
                       lat,
                       lon,
                       effective_tz,
                       &sol)) {
        console_puts("SOLAR: UNAVAILABLE\n");
        return;
    }
}



// src/console/console_cmds.cpp

static void cmd_set(int argc, char **argv)
{
    ensure_cfg_loaded();

    if (argc < 3) {
        console_puts("?\n");
        return;
    }

    // set date YYYY-MM-DD
    if (!strcmp(argv[1], "date") && argc == 3) {
        int yy, mm, dd;
        if (!parse_date_ymd(argv[2], &yy, &mm, &dd)) {
            console_puts("ERROR\n");
            return;
        }
        g_date_y = yy;
        g_date_mo = mm;
        g_date_d = dd;
        g_have_date = true;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set time HH:MM[:SS]
    if (!strcmp(argv[1], "time") && argc == 3) {
        int hh = 0, mi = 0, ss = 0;

        if (parse_time_hms(argv[2], &hh, &mi, &ss)) {
            /* HH:MM:SS */
        } else if (parse_time_hm(argv[2], &hh, &mi)) {
            /* HH:MM — default seconds */
            ss = 0;
        } else {
            console_puts("ERROR\n");
            return;
        }

        g_time_h = hh;
        g_time_m = mi;
        g_time_s = ss;

        g_have_time = true;
        g_time_set_uptime_s = uptime_seconds();
        g_cfg_dirty = true;

        console_puts("OK\n");
        return;
    }

    // set lat +/-DD.DDDD
    if (!strcmp(argv[1], "lat") && argc == 3) {
        float v = atof(argv[2]);
        if (v < -90.0f || v > 90.0f) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.latitude_e4 = (int32_t)(v * 10000.0f);
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set lon +/-DDD.DDDD
    if (!strcmp(argv[1], "lon") && argc == 3) {
        float v = atof(argv[2]);
        if (v < -180.0f || v > 180.0f) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.longitude_e4 = (int32_t)(v * 10000.0f);
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set tz +/-HH
    if (!strcmp(argv[1], "tz") && argc == 3) {
        int v = atoi(argv[2]);
        if (v < -12 || v > 14) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.tz = v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set dst on|off
    if (!strcmp(argv[1], "dst") && argc == 3) {
        if (!strcmp(argv[2], "on")) {
            g_cfg.honor_dst = true;
            g_cfg_dirty = true;
            console_puts("OK\n");
            return;
        }
        if (!strcmp(argv[2], "off")) {
            g_cfg.honor_dst = false;
            g_cfg_dirty = true;
            console_puts("OK\n");
            return;
        }
        console_puts("ERROR\n");
        return;
    }

    console_puts("?\n");
}

/*
 * Parse a device state from user input.
 *
 * Rules:
 *  - Prefer device-defined state_string()
 *  - Case-insensitive match
 *  - Fallback to "on"/"off"
 *  - No scheduler or device knowledge here
 */

 static bool parse_device_state(const Device *d,
                                const char *arg,
                                dev_state_t *out)
 {
     if (!d || !arg || !out)
         return false;

     /* Device-specific names first */
     if (d->state_string) {
         for (dev_state_t s = DEV_STATE_UNKNOWN;
              s <= DEV_STATE_ON;
              s = (dev_state_t)(s + 1)) {

             const char *name = d->state_string(s);
             if (!name)
                 continue;

             if (!strcasecmp(arg, name)) {
                 *out = s;
                 return true;
             }
         }
     }

     /* Fallback */
     if (!strcasecmp(arg, "on")) {
         *out = DEV_STATE_ON;
         return true;
     }

     if (!strcasecmp(arg, "off")) {
         *out = DEV_STATE_OFF;
         return true;
     }

     return false;
 }

 static void cmd_device(int argc, char **argv)
 {
     /* device */
     if (argc == 1) {
         for (size_t i = 0; i < device_count; i++) {
             const Device *d = devices[i];
             if (!d)
                 continue;

             dev_state_t st = DEV_STATE_UNKNOWN;
             if (d->get_state)
                 st = d->get_state();

             const char *s = "?";
             if (d->state_string)
                 s = d->state_string(st);

             console_puts(d->name);
             console_puts(": ");
             console_puts(s);
             console_putc('\n');
         }
         return;
     }

     /* device <name> <state> */
     if (argc == 3) {
         uint8_t id;

         /* NOTE: 0 == success */
         if (device_lookup_id(argv[1], &id) == 0) {
             console_puts("ERROR\n");
             return;
         }

         const Device *d = devices[id];
         if (!d || !d->set_state) {
             console_puts("ERROR\n");
             return;
         }

         dev_state_t want;
         if (!parse_device_state(d, argv[2], &want)) {
             console_puts("ERROR\n");
             return;
         }

         d->set_state(want);
         console_puts("OK\n");
         return;
     }

     console_puts("?\n");
 }


static void cmd_save(int argc,char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    if (!g_have_date || !g_have_time) {
        console_puts("ERROR: DATE/TIME NOT SET\n");
        return;
    }

    // Option B: compensate for operator delay between 'set time' and 'save'.
    uint32_t now_s = uptime_seconds();
    uint32_t delta_s = now_s - g_time_set_uptime_s;

    int y = g_date_y;
    int mo = g_date_mo;
    int d = g_date_d;
    int h = g_time_h;
    int m = g_time_m;
    int s = g_time_s;

    // Add elapsed seconds (delta_s can be large, so use division/mod).
    uint32_t add = delta_s;
    s += (int)(add % 60);
    add /= 60;
    m += (int)(add % 60);
    add /= 60;
    h += (int)(add % 24);
    add /= 24;

    // Normalize seconds/minutes/hours
    while (s >= 60) { s -= 60; m++; }
    while (m >= 60) { m -= 60; h++; }
    while (h >= 24) { h -= 24; add++; }

    // Advance days
    while (add--) {
        advance_one_day(&y, &mo, &d);
    }

    // Program RTC once on commit.
    rtc_set_time(y, mo, d, h, m, s);

    // Commit persistent configuration.
    config_save(&g_cfg);
    g_cfg_dirty = false;

    // Update shadow time to the committed instant.
    g_date_y = y; g_date_mo = mo; g_date_d = d;
    g_time_h = h; g_time_m = m; g_time_s = s;
    g_time_set_uptime_s = now_s;

    console_puts("OK\n");
}

static void cmd_door(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: door open|close\n");
        return;
    }

    if (!strcmp(argv[1], "open")) {
        console_puts("DOOR: open\n");
        door_open();
        return;
    }

    if (!strcmp(argv[1], "close")) {
        console_puts("DOOR: close\n");
        door_close();
        return;
    }

    console_puts("?\n");
}

// src/console/console_cmds.cpp

static void cmd_lock(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: lock engage|release\n");
        return;
    }

    if (!strcmp(argv[1], "engage")) {
        console_puts("LOCK: engage\n");
        lock_engage();
        return;
    }

    if (!strcmp(argv[1], "release")) {
        console_puts("LOCK: release\n");
        lock_release();
        return;
    }

    console_puts("?\n");
}

static void cmd_led(int argc, char **argv)
{
    if (argc != 2) {
        console_puts("usage: led off|red|green|pulse_red|pulse_green|blink_red|blink_green\n");
        return;
    }

    const char *s = argv[1];

    if (!strcmp(s, "off"))            door_led_set(DOOR_LED_OFF);
    else if (!strcmp(s, "red"))       door_led_set(DOOR_LED_RED);
    else if (!strcmp(s, "green"))     door_led_set(DOOR_LED_GREEN);
    else if (!strcmp(s, "pulse_red")) door_led_set(DOOR_LED_PULSE_RED);
    else if (!strcmp(s, "pulse_green")) door_led_set(DOOR_LED_PULSE_GREEN);
    else if (!strcmp(s, "blink_red")) door_led_set(DOOR_LED_BLINK_RED);
    else if (!strcmp(s, "blink_green")) door_led_set(DOOR_LED_BLINK_GREEN);
    else {
        console_puts("ERROR\n");
        return;
    }

    console_puts("OK\n");
}

static void cmd_config(int, char **)
{
    ensure_cfg_loaded();

#ifdef HOST_BUILD
    /*
     * Host initialization only:
     * If no staged date/time exists, seed shadow from RTC once.
     * Never overwrite staged intent.
     */
    if (rtc_time_is_set() && (!g_have_date || !g_have_time)) {
        rtc_get_time(&g_date_y, &g_date_mo, &g_date_d,
                     &g_time_h, &g_time_m, &g_time_s);
        g_have_date = true;
        g_have_time = true;
        g_time_set_uptime_s = uptime_seconds();
    }
#endif

    if (g_cfg_dirty)
        console_puts("CONFIG (UNSAVED)\n\n");
    else
        console_puts("CONFIG (SAVED)\n\n");

    /* date */
    console_puts("date : ");
    if (g_have_date)
        mini_printf("%04d-%02d-%02d\n", g_date_y, g_date_mo, g_date_d);
    else
        console_puts("NOT SET\n");

    /* time */
    console_puts("time : ");
    if (g_have_date && g_have_time) {

        int y  = g_date_y;
        int mo = g_date_mo;
        int d  = g_date_d;
        int h  = g_time_h;
        int m  = g_time_m;
        int s  = g_time_s;

        uint32_t now_s   = uptime_seconds();
        uint32_t delta_s = now_s - g_time_set_uptime_s;

        /* Apply elapsed time (same math as save, display-only) */
        s += (int)(delta_s % 60);
        delta_s /= 60;
        m += (int)(delta_s % 60);
        delta_s /= 60;
        h += (int)(delta_s % 24);
        delta_s /= 24;

        while (s >= 60) { s -= 60; m++; }
        while (m >= 60) { m -= 60; h++; }
        while (h >= 24) { h -= 24; delta_s++; }

        while (delta_s--) {
            advance_one_day(&y, &mo, &d);
        }

        mini_printf("%02d:%02d:%02d\n", h, m, s);

    } else {
        console_puts("NOT SET\n");
    }

    /* lat / lon / tz */
    mini_printf("lat  : %L\n", g_cfg.latitude_e4);
    mini_printf("lon  : %L\n", g_cfg.longitude_e4);
    mini_printf("tz   : %d\n",   g_cfg.tz);

    mini_printf("dst  : %s\n",
                g_cfg.honor_dst ? "ON (US rules)" : "OFF");

    console_putc('\n');
}



void console_help(int argc, char **argv);

static void cmd_run(int,char**)
{
    console_puts("Leaving CONFIG mode\n");
    want_exit = true;
}

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

         /* Resolve + collect (index is the stable ID) */
         struct Resolved {
             uint16_t minute;
             uint8_t  index;
         };

         struct Resolved r[MAX_EVENTS];
         size_t rcount = 0;

         for (size_t i = 0; i < count; i++) {
             uint16_t minute;
             if (!resolve_when(&events[i].when,
                               have_sol ? &sol : NULL,
                               &minute))
                 continue;

             r[rcount].minute = minute;
             r[rcount].index  = (uint8_t)i;
             rcount++;
         }

         if (rcount == 0) {
             console_puts("(no events)\n");
             return;
         }

         /* Sort by resolved time, then index */
         for (size_t i = 0; i + 1 < rcount; i++) {
             for (size_t j = i + 1; j < rcount; j++) {
                 if (r[j].minute < r[i].minute ||
                    (r[j].minute == r[i].minute &&
                     r[j].index < r[i].index)) {

                     struct Resolved tmp = r[i];
                     r[i] = r[j];
                     r[j] = tmp;
                 }
             }
         }

         /* Print */
         for (size_t i = 0; i < rcount; i++) {
             const Event *ev = &events[r[i].index];
             uint16_t minute = r[i].minute;

             const char *dev = "?";
             if (ev->device_id < device_count) {
                 const Device *d = devices[ev->device_id];
                 if (d && d->name)
                     dev = d->name;
             }

             /* Time first, index secondary */
             mini_printf("%02u:%02u #%u ",
                         (unsigned)(minute / 60),
                         (unsigned)(minute % 60),
                         (unsigned)r[i].index);

             console_puts(dev);
             console_putc(' ');
             console_puts(action_name(ev->action));
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
     * event delete <index>
     * ------------------------------------------------------------------ */
    if (!strcmp(argv[1], "delete") && argc == 3) {

        char *end = NULL;
        long idx = strtol(argv[2], &end, 10);

        if (!end || *end != '\0' || idx < 0 || idx >= (long)MAX_EVENTS) {
            console_puts("ERROR\n");
            return;
        }

        if (!config_events_delete((uint8_t)idx)) {
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
            console_puts("ERROR\n");
            return;
        }

        if (!device_lookup_id(argv[2], &ev.device_id)) {
            console_puts("ERROR\n");
            return;
        }

        if (!strcmp(argv[3], "on"))
            ev.action = ACTION_ON;
        else if (!strcmp(argv[3], "off"))
            ev.action = ACTION_OFF;
        else {
            console_puts("ERROR\n");
            return;
        }

        /* implicit midnight: HH:MM */
        if (argc == 5) {
            int hh, mm;
            if (!parse_time_hm(argv[4], &hh, &mm)) {
                console_puts("ERROR\n");
                return;
            }
            ev.when.ref = REF_MIDNIGHT;
            ev.when.offset_minutes = (int16_t)(hh * 60 + mm);
            goto add_event;
        }

        /* explicit midnight */
        if (argc == 6 && !strcmp(argv[4], "midnight")) {
            int hh, mm;
            if (!parse_time_hm(argv[5], &hh, &mm)) {
                console_puts("ERROR\n");
                return;
            }
            ev.when.ref = REF_MIDNIGHT;
            ev.when.offset_minutes = (int16_t)(hh * 60 + mm);
            goto add_event;
        }

        /* solar anchors */
        if (argc == 7 && !strcmp(argv[4], "solar")) {

            if (!strcmp(argv[5], "sunrise"))
                ev.when.ref = REF_SOLAR_STD_RISE;
            else if (!strcmp(argv[5], "sunset"))
                ev.when.ref = REF_SOLAR_STD_SET;
            else {
                console_puts("ERROR\n");
                return;
            }

            int off;
            if (!parse_signed_int(argv[6], &off)) {
                console_puts("ERROR\n");
                return;
            }

            ev.when.offset_minutes = (int16_t)off;
            goto add_event;
        }

        /* civil anchors */
        if (argc == 7 && !strcmp(argv[4], "civil")) {

            if (!strcmp(argv[5], "dawn"))
                ev.when.ref = REF_SOLAR_CIV_RISE;
            else if (!strcmp(argv[5], "dusk"))
                ev.when.ref = REF_SOLAR_CIV_SET;
            else {
                console_puts("ERROR\n");
                return;
            }

            int off;
            if (!parse_signed_int(argv[6], &off)) {
                console_puts("ERROR\n");
                return;
            }

            ev.when.offset_minutes = (int16_t)off;
            goto add_event;
        }

        console_puts("ERROR\n");
        return;

add_event:ev.refnum = 0;

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

static void cmd_next(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    uint16_t now = rtc_minutes_since_midnight();

    /* Compute today's solar times */
    struct solar_times sol;
    bool have_sol = false;

    double lat = (double)g_cfg.latitude_e4  / 10000.0;
    double lon = (double)g_cfg.longitude_e4 / 10000.0;

    int tz = g_cfg.tz;
    if (g_cfg.honor_dst &&
        is_us_dst(g_date_y, g_date_mo, g_date_d, g_time_h)) {
        tz += 1;
    }

    have_sol = solar_compute(
        g_date_y,
        g_date_mo,
        g_date_d,
        lat,
        lon,
        (int8_t)tz,
        &sol
    );

    size_t count = 0;
    const Event *events = config_events_get(&count);

    if (count == 0) {
        console_puts("next: none\n");
        return;
    }

    size_t idx = 0;
    uint16_t minute = 0;
    bool tomorrow = false;

    if (!next_event_today(events,
                          count,
                          have_sol ? &sol : NULL,
                          now,
                          &idx,
                          &minute,
                          &tomorrow)) {
        console_puts("next: none\n");
        return;
    }

    int32_t delta_min;
    if (tomorrow)
        delta_min = (1440 - now) + minute;
    else
        delta_min = (int32_t)minute - (int32_t)now;

    console_puts(tomorrow ? "next: tomorrow " : "next: ");

    mini_printf("%02u:%02u (+%d min) ",
                (unsigned)(minute / 60),
                (unsigned)(minute % 60),
                (int)delta_min);

    const Event *ev = &events[idx];

    const char *dev = "?";
    if (ev->device_id < device_count) {
        const Device *d = devices[ev->device_id];
        if (d && d->name)
            dev = d->name;
    }

    console_puts(dev);
    console_putc(' ');
    console_puts(action_name(ev->action));
    console_putc(' ');
    when_print(&ev->when);
    console_putc('\n');
}

#ifdef HOST_BUILD

static void cmd_sleep(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    console_puts("sleep: not yet implemented\n");
}

#endif


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
     X(timeout, 1, 1, cmd_timeout, \
       "Control CONFIG timeout", \
       "timeout on\n" \
       "timeout off\n" \
       "  Enable or disable CONFIG inactivity timeout\n" \
     ) \
     \
     X(device, 0, 3, cmd_device, \
        "Show or set device state", \
        "device\n" \
        "device <name>\n" \
        "device <name> on|off\n" \
        "  Show all device states, show one device, or set device state\n" \
      ) \
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
     X(exit, 0, 0, cmd_run, \
       "Leave config mode", \
       "exit\n" \
       "  Leave CONFIG mode\n" \
     ) \
     X(next, 0, 0, cmd_next, \
       "Show next scheduled event", \
       "next\n" \
       "  Display the next resolved scheduler event (if any)\n" \
     ) \
     X(event, 0, 7, cmd_event, \
        "Event commands", \
        "event list\n" \
        "event add <device> <on|off> HH:MM\n" \
        "event add <device> <on|off> midnight HH:MM\n" \
        "event add <device> <on|off> solar sunrise +/-MIN\n" \
        "event add <device> <on|off> solar sunset  +/-MIN\n" \
        "event add <device> <on|off> civil dawn    +/-MIN\n" \
        "event add <device> <on|off> civil dusk    +/-MIN\n" \
        "event delete <index>" \
      ) \
      X(led, 1, 1, cmd_led, \
        "Control door LED", \
        "led off\n" \
        "led red\n" \
        "led green\n" \
        "led pulse_red\n" \
        "led pulse_green\n" \
        "led blink_red\n" \
        "led blink_green\n" \
      )

 /* Commands that only exist when their handlers exist */
 #ifdef HOST_BUILD
 #define CMD_SLEEP_HOST(X) \
     X(sleep, 0, 0, cmd_sleep, \
       "Sleep til next scheduled event", \
       "sleep\n" \
       "  sleep till the next resolved scheduler event (if any)\n" \
     )
 #else
 #define CMD_SLEEP_HOST(X)
 #endif

 #ifdef __AVR__
 #include <avr/pgmspace.h>
 #define DECLARE_CMD_STRINGS(name, min, max, fn, short_h, long_h) \
     static const char cmd_##name##_name[] PROGMEM = #name; \
     static const char cmd_##name##_short[] PROGMEM = short_h; \
     static const char cmd_##name##_long[] PROGMEM = long_h;
 #else
 #define DECLARE_CMD_STRINGS(name, min, max, fn, short_h, long_h) \
     static const char *cmd_##name##_name = #name; \
     static const char *cmd_##name##_short = short_h; \
     static const char *cmd_##name##_long = long_h;
 #endif

 CMD_LIST(DECLARE_CMD_STRINGS)
 CMD_SLEEP_HOST(DECLARE_CMD_STRINGS)

 #ifdef __AVR__
 #define CMD_PROGMEM PROGMEM
 #else
 #define CMD_PROGMEM
 #endif

 static const cmd_entry_t cmd_table[] CMD_PROGMEM = {
 #define MAKE_CMD_ENTRY(name, min, max, fn, short_h, long_h) \
     { cmd_##name##_name, min, max, fn, cmd_##name##_short, cmd_##name##_long },
     CMD_LIST(MAKE_CMD_ENTRY) \
     CMD_SLEEP_HOST(MAKE_CMD_ENTRY)
 #undef MAKE_CMD_ENTRY
 };

 #define CMD_TABLE_LEN (sizeof(cmd_table) / sizeof(cmd_table[0]))

 /// -------------------------

void console_help(int argc, char **argv)
{
    /* help */
    if (argc == 1) {
        console_puts("Commands:\n");

        unsigned max_len = 0;
        for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
            unsigned len = strlen(cmd_table[i].cmd);
            if (len > max_len)
                max_len = len;
        }

        for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
            console_puts("  ");
            console_puts(cmd_table[i].cmd);

            unsigned len = strlen(cmd_table[i].cmd);
            while (len++ < max_len + 2)
                console_putc(' ');

            console_puts(cmd_table[i].help_short);
            console_putc('\n');
        }

        console_puts("\nType: help <command>\n");
        return;
    }

    /* help <command> */
    for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
        if (!strcmp(argv[1], cmd_table[i].cmd)) {
            if (cmd_table[i].help_long)
                console_puts(cmd_table[i].help_long);
            return;
        }
    }

    console_puts("?\n");
}


// src/console/console_cmds.cpp

void console_dispatch(int argc, char **argv)
{
    if (argc == 0)
        return;

    str_to_lower(argv[0]);

    for (unsigned i = 0; i < CMD_TABLE_LEN; i++) {
        if (!strcmp(argv[0], cmd_table[i].cmd)) {

            int args = argc - 1;

            if (args < cmd_table[i].min_args ||
                args > cmd_table[i].max_args) {

                if (cmd_table[i].help_short) {
                    console_puts(cmd_table[i].help_short);
                    console_putc('\n');
                }
                return;
            }

            cmd_table[i].handler(argc, argv);
            return;
        }
    }

    console_puts("?\n");
}
