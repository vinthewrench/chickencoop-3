
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

 #include <string.h>
 #include <stdlib.h>
 #include <ctype.h>


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
#include "state_reducer.h"

extern bool want_exit;

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
static void cmd_timeout(int argc, char **argv);
static void cmd_door(int argc, char **argv);
static void cmd_lock(int argc, char **argv);
static void cmd_run(int argc, char **argv);
static void cmd_event(int argc, char **argv);

#ifdef HOST_BUILD
static void cmd_next(int argc, char **argv);
static void cmd_reduce(int argc, char **argv);
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

    /* RTC is the sole authority */
    if (!rtc_time_is_set())
        return false;

    rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

    double lat = (double)g_cfg.latitude_e4 / 10000.0;
    double lon = (double)g_cfg.longitude_e4 / 10000.0;

    int tz = g_cfg.tz;
    if (g_cfg.honor_dst && is_us_dst(y, mo, d, h)) {
        tz += 1;
    }

    return solar_compute(
        y,
        mo,
        d,
        lat,
        lon,
        (int8_t)tz,
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

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    int y, mo, d, h;
    rtc_get_time(&y, &mo, &d, &h, NULL, NULL);

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
       console_suspend_timeout();
       console_puts("TIMEOUT DISABLED\n");
        return;
    }

    if (!strcmp(argv[1], "on")) {
        console_resume_timeout();
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

    /* ----- Date / time ----- */
    int y, mo, d, h, m, s;

    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }

    rtc_get_time(&y, &mo, &d, &h, &m, &s);

    /* ----- Header ----- */
    mini_printf("Today: %04d-%02d-%02d\n\n", y, mo, d);

    mini_printf("lat/long  : %L, %L\n",
                g_cfg.latitude_e4,
                g_cfg.longitude_e4);

     mini_printf("TZ        : %d (DST ", g_cfg.tz);
     mini_printf("%s", g_cfg.honor_dst ? "ON" : "OFF");
     mini_printf(")\n\n");

    /* ----- Solar ----- */
    struct solar_times sol;
    bool have_sol = compute_today_solar(&sol);

    if (have_sol) {
        console_puts("Solar      Rise        Set\n");
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
    } else {
        console_puts("Solar: UNAVAILABLE\n");
    }

    console_putc('\n');
    console_puts("Events:\n");

    /* ----- Events ----- */
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
        if (!resolve_when(&ev->when, have_sol ? &sol : NULL, &minute))
            continue;

        rows[rc].minute = minute;
        rows[rc].ev     = ev;
        rc++;
    }

    if (rc == 0) {
        console_puts("(no resolvable events)\n");
        return;
    }

    /* sort by time */
    for (size_t i = 0; i + 1 < rc; i++) {
        for (size_t j = i + 1; j < rc; j++) {
            if (rows[j].minute < rows[i].minute) {
                struct Row t = rows[i];
                rows[i] = rows[j];
                rows[j] = t;
            }
        }
    }

    /* print rows */
    for (size_t i = 0; i < rc; i++) {
        const Event *ev = rows[i].ev;
        uint16_t min = rows[i].minute;

        const char *dev = "?";
        const char *state = "?";

        device_name(ev->device_id, &dev);

        dev_state_t st =
            (ev->action == ACTION_ON) ? DEV_STATE_ON : DEV_STATE_OFF;

        device_get_state_string(ev->device_id, st, &state);

        mini_printf("%02u:%02u  ",
            (unsigned)(min / 60),
            (unsigned)(min % 60));

        print_padded(dev,   8);
        print_padded(state, 8);

        when_print(&ev->when);
        console_putc('\n');
    }
}


// src/console/console_cmds.cpp
// src/console/console_cmds.cpp

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

        /* RTC must already have a valid time */
        if (!rtc_time_is_set()) {
            console_puts("ERROR: RTC TIME NOT SET\n");
            return;
        }

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
            /* HH:MM:SS */
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

        /* Preserve existing RTC date */
        rtc_get_time(&y, &mo, &d, NULL, NULL, NULL);

        /* Program RTC */
        if (!rtc_set_time(y, mo, d, hh, mi, ss)) {
            console_puts("ERROR: RTC SET FAILED\n");
            return;
        }

        /* Record UTC epoch at moment of set */
        g_cfg.rtc_set_epoch =
            rtc_epoch_from_ymdhms(
                y, mo, d,
                hh, mi, ss,
                g_cfg.tz,
                g_cfg.honor_dst
            );

        /* Immediately persist drift baseline */
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
        console_puts("usage: door open|close\n");
        return;
    }

    /* Map door → device door on|off */
    char *dev_argv[3];
    dev_argv[0] = (char *)"device";
    dev_argv[1] = (char *)"door";

    if (!strcmp(argv[1], "open")) {
        dev_argv[2] = (char *)"on";
    }
    else if (!strcmp(argv[1], "close")) {
        dev_argv[2] = (char *)"off";
    }
    else {
        console_puts("?\n");
        return;
    }

    /* Delegate to canonical implementation */
    cmd_device(3, dev_argv);
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
    rtc_get_time(&y, &mo, &d, &h, &m, &s);
    uint32_t epoch = rtc_get_epoch();

    console_puts("RTC: VALID\n");
    mini_printf("date: %04d-%02d-%02d\n", y, mo, d);
    mini_printf("time: %02d:%02d:%02d\n", h, m, s);
    mini_printf("epoch: %lu\n", (unsigned long)epoch);

    uint16_t mins = rtc_minutes_since_midnight();
    mini_printf("minutes_since_midnight: %u\n", (unsigned)mins);

    /* --------------------------------------------------
        * Drift measurement
        * -------------------------------------------------- */
       if (g_cfg.rtc_set_epoch == 0) {
           console_puts("since_set: UNKNOWN\n");
           return;
       }

       uint32_t delta;

       if (epoch >= g_cfg.rtc_set_epoch) {
           delta = epoch - g_cfg.rtc_set_epoch;
       } else {
           /* Should never happen, but stay deterministic */
           delta = 0;
       }

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
            uint16_t minute = r[i].minute;

            const char *dev_name = "?";
            const char *state    = "?";

            device_name(ev->device_id, &dev_name);

            dev_state_t st =
                (ev->action == ACTION_ON) ? DEV_STATE_ON : DEV_STATE_OFF;

            device_get_state_string(ev->device_id, st, &state);

            /* Time and stable refnum */
            mini_printf("%02u:%02u  #",
                        (unsigned)(minute / 60),
                        (unsigned)(minute % 60));

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
                 ev.when.ref = REF_MIDNIGHT;
                 ev.when.offset_minutes = (int16_t)(hh * 60 + mm);
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
             ev.when.ref = REF_MIDNIGHT;
             ev.when.offset_minutes = (int16_t)(hh * 60 + mm);
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

/* -------------------------------------------------------------------------- */
/* Host-only scheduler diagnostics                                             */
/* -------------------------------------------------------------------------- */

#ifdef HOST_BUILD

static void cmd_next(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    uint16_t now = rtc_minutes_since_midnight();

    struct solar_times sol;
    bool have_sol = compute_today_solar(&sol);

    size_t used = 0;
    const Event *events = config_events_get(&used);

    size_t idx = 0;
    uint16_t minute = 0;
    bool tomorrow = false;

    if (!next_event_today(events,
                          MAX_EVENTS,
                          have_sol ? &sol : NULL,
                          now,
                          &idx,
                          &minute,
                          &tomorrow)) {
        console_puts("next: none\n");
        return;
    }

    int32_t delta =
        tomorrow ? (1440 - now + minute) : ((int32_t)minute - now);

    mini_printf("next: %02u:%02u (+%ld min) ",
                minute / 60, minute % 60, (long)delta);

    const Event *ev = &events[idx];

    const char *name = "?";
    device_name(ev->device_id, &name);

    console_puts(name);
    console_putc(' ');
    console_puts(ev->action == ACTION_ON ? "on " : "off ");
    when_print(&ev->when);
    console_putc('\n');
}

static void cmd_reduce(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    ensure_cfg_loaded();

    uint16_t now = rtc_minutes_since_midnight();

    struct solar_times sol;
    bool have_sol = compute_today_solar(&sol);

    size_t used = 0;
    const Event *events = config_events_get(&used);

    struct reduced_state rs;
    state_reducer_run(events,
                      MAX_EVENTS,
                      have_sol ? &sol : NULL,
                      now,
                      &rs);

    uint8_t id;
    bool any = false;

    for (bool ok = device_enum_first(&id);
         ok;
         ok = device_enum_next(id, &id)) {

        if (!rs.has_action[id])
            continue;

        const char *name = "?";
        const char *state = "?";

        device_name(id, &name);

        dev_state_t st =
            (rs.action[id] == ACTION_ON) ? DEV_STATE_ON : DEV_STATE_OFF;

        device_get_state_string(id, st, &state);

        console_puts(name);
        console_puts(": ");
        console_puts(state);
        console_putc('\n');
        any = true;
    }

    if (!any)
        console_puts("(no scheduled state)\n");
}

static void cmd_sleep(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    console_puts("sleep: not yet implemented\n");
}

#endif /* HOST_BUILD */


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
    X(exit, 0, 0, cmd_run, \
      "Leave config mode", \
      "exit\n" \
      "  Leave CONFIG mode\n" \
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
    )

/* ------------------------------------------------------------
 * Host-only commands
 * ------------------------------------------------------------ */

#ifdef HOST_BUILD
#define CMD_SCHED_HOST(X) \
    X(next, 0, 0, cmd_next, \
      "Show next scheduled event", \
      "next\n" \
      "  Display the next resolved scheduler event (if any)\n" \
    ) \
    X(reduce, 0, 0, cmd_reduce, \
      "Reduce schedule to expected device state", \
      "reduce\n" \
      "  Show the scheduler-reduced expected state for each device\n" \
      "  at the current RTC time. No execution is performed.\n" \
    )
#else
#define CMD_SCHED_HOST(X)
#endif

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

/* ------------------------------------------------------------
 * Command string declarations
 * ------------------------------------------------------------ */

#ifdef __AVR__
#include <avr/pgmspace.h>
#define DECLARE_CMD_STRINGS(name, min, max, fn, short_h, long_h) \
    static const char cmd_##name##_name[] PROGMEM = #name; \
    static const char cmd_##name##_short[] PROGMEM = short_h; \
    static const char cmd_##name##_long[]  PROGMEM = long_h;
#else
#define DECLARE_CMD_STRINGS(name, min, max, fn, short_h, long_h) \
    static const char *cmd_##name##_name  = #name; \
    static const char *cmd_##name##_short = short_h; \
    static const char *cmd_##name##_long  = long_h;
#endif

CMD_LIST(DECLARE_CMD_STRINGS)
CMD_SCHED_HOST(DECLARE_CMD_STRINGS)
CMD_SLEEP_HOST(DECLARE_CMD_STRINGS)

/* ------------------------------------------------------------
 * Command table
 * ------------------------------------------------------------ */

#ifdef __AVR__
#define CMD_PROGMEM PROGMEM
#else
#define CMD_PROGMEM
#endif

static const cmd_entry_t cmd_table[] CMD_PROGMEM = {
#define MAKE_CMD_ENTRY(name, min, max, fn, short_h, long_h) \
    { cmd_##name##_name, min, max, fn, cmd_##name##_short, cmd_##name##_long },
    CMD_LIST(MAKE_CMD_ENTRY)
    CMD_SCHED_HOST(MAKE_CMD_ENTRY)
    CMD_SLEEP_HOST(MAKE_CMD_ENTRY)
#undef MAKE_CMD_ENTRY
};

#define CMD_TABLE_LEN (sizeof(cmd_table) / sizeof(cmd_table[0]))

#ifdef __AVR__
static void read_cmd_entry(cmd_entry_t *dst, unsigned idx)
{
    memcpy_P(dst, &cmd_table[idx], sizeof(cmd_entry_t));
}
#else
static void read_cmd_entry(cmd_entry_t *dst, unsigned idx)
{
    *dst = cmd_table[idx];
}
#endif

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
