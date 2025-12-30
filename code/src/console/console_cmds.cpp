
// src/console/console_cmds.cpp

#include "console/console_io.h"
#include "console/console.h"
#include "console/mini_printf.h"
#include "time_dst.h"
#include "console_time.h"

#include "events.h"
#include "solar.h"
#include "rtc.h"
#include "config.h"
#include "lock.h"
#include "door.h"
#include "uptime.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

extern bool want_exit;

// -----------------------------------------------------------------------------
// CONFIG shadow state
//   - set commands modify RAM shadow only
//   - save commits to EEPROM and programs RTC
// -----------------------------------------------------------------------------

static struct config g_cfg;
static bool g_cfg_loaded = false;
static bool g_cfg_dirty = false;

static int g_date_y = 0, g_date_mo = 0, g_date_d = 0;
static int g_time_h = 0, g_time_m = 0, g_time_s = 0;
static bool g_have_date = false;
static bool g_have_time = false;
static uint32_t g_time_set_uptime_s = 0;


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
    if (w->ref == REF_NONE) {
        console_puts("DISABLED");
        return;
    }

    if (w->ref == REF_MIDNIGHT) {
        int h = w->offset_minutes / 60;
        int m = abs(w->offset_minutes % 60);
        mini_printf("%02d:%02d", h, m);
        return;
    }

    const char *name =
        (w->ref == REF_SOLAR_STD) ? "SOLAR" :
        (w->ref == REF_SOLAR_CIV) ? "CIVIL" :
        "?";

    mini_printf("%s %c%d",
        name,
        (w->offset_minutes < 0) ? '-' : '+',
        abs(w->offset_minutes));
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
    int y,mo,d,h,m,s;
    if (!rtc_time_is_set()) {
        console_puts("TIME: NOT SET\n");
        return;
    }
    rtc_get_time(&y,&mo,&d,&h,&m,&s);
    print_datetime_ampm(y,mo,d,h,m,s);
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

    struct solar_times sol;
    if (!solar_compute(y, mo, d,
                       g_cfg.latitude,
                       g_cfg.longitude,
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

    if (!solar_compute(y, mo, d,
                       g_cfg.latitude,
                       g_cfg.longitude,
                       effective_tz,
                       &sol)) {
        console_puts("SOLAR: UNAVAILABLE\n");
        return;
    }

    console_puts("OPEN : ");
    when_print(&g_cfg.door.open_when);
    console_putc('\n');

    console_puts("CLOSE: ");
    when_print(&g_cfg.door.close_when);
    console_putc('\n');
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

    // set time HH:MM:SS
    if (!strcmp(argv[1], "time") && argc == 3) {
        int hh, mi, ss;
        if (!parse_time_hms(argv[2], &hh, &mi, &ss)) {
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
        double v = atof(argv[2]);
        if (v < -90.0 || v > 90.0) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.latitude = v;
        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set lon +/-DDD.DDDD
    if (!strcmp(argv[1], "lon") && argc == 3) {
        double v = atof(argv[2]);
        if (v < -180.0 || v > 180.0) {
            console_puts("ERROR\n");
            return;
        }
        g_cfg.longitude = v;
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

    // set door open|close HH:MM
    if (!strcmp(argv[1], "door") && argc == 4) {
        bool is_open = !strcmp(argv[2], "open");
        bool is_close = !strcmp(argv[2], "close");

        if (!is_open && !is_close) {
            console_puts("?\n");
            return;
        }

        int hh, mm;
        if (!parse_time_hm(argv[3], &hh, &mm)) {
            console_puts("ERROR\n");
            return;
        }

        struct When *w = is_open
            ? &g_cfg.door.open_when
            : &g_cfg.door.close_when;

        w->ref = REF_MIDNIGHT;
        w->offset_minutes = (int16_t)(hh * 60 + mm);

        g_cfg_dirty = true;
        console_puts("OK\n");
        return;
    }

    // set door open|close solar|civil +/-MIN
    if (!strcmp(argv[1], "door") && argc == 5) {
        bool is_open = !strcmp(argv[2], "open");
        bool is_close = !strcmp(argv[2], "close");

        if (!is_open && !is_close) {
            console_puts("?\n");
            return;
        }

        struct When *w = is_open
            ? &g_cfg.door.open_when
            : &g_cfg.door.close_when;

        int offset = atoi(argv[4]);

        if (!strcmp(argv[3], "solar")) {
            w->ref = REF_SOLAR_STD;
            w->offset_minutes = (int16_t)offset;
        }
        else if (!strcmp(argv[3], "civil")) {
            w->ref = REF_SOLAR_CIV;
            w->offset_minutes = (int16_t)offset;
        }
        else {
            console_puts("?\n");
            return;
        }

        g_cfg_dirty = true;
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


static void cmd_config(int, char **)
{
    ensure_cfg_loaded();

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
    if (g_have_time)
        mini_printf("%02d:%02d:%02d\n", g_time_h, g_time_m, g_time_s);
    else
        console_puts("NOT SET\n");

    /* lat / lon / tz */
    mini_printf("lat  : %L\n", g_cfg.latitude);
    mini_printf("lon  : %L\n", g_cfg.longitude);
    mini_printf("tz   : %d\n",   g_cfg.tz);

    mini_printf("dst  : %s\n",
                g_cfg.honor_dst ? "ON (US rules)" : "OFF");

    console_putc('\n');

    console_puts("door open  : ");
    when_print(&g_cfg.door.open_when);
    console_putc('\n');

    console_puts("door close : ");
    when_print(&g_cfg.door.close_when);
    console_putc('\n');
}



void console_help(int argc, char **argv);

static void cmd_run(int,char**)
{
    console_puts("Leaving CONFIG mode\n");
    want_exit = true;
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


static const cmd_entry_t cmd_table[] = {

    { "help", 0, 1, console_help,
      "Show help",
      "help\n"
      "help <command>\n"
      "  Show top-level command list or detailed help for a command\n"
    },

    { "version", 0, 0, cmd_version,
      "Show firmware version",
      "version\n"
      "  Show firmware version and build date\n"
    },

    { "time", 0, 0, cmd_time,
      "Show current date/time",
      "time\n"
      "  Show RTC date and time\n"
      "  Format: YYYY-MM-DD HH:MM:SS AM|PM\n"
    },

    { "schedule", 0, 0, cmd_schedule,
      "Show schedule",
      "schedule\n"
      "  Show system schedule and next resolved events\n"
    },

    { "solar", 0, 0, cmd_solar,
      "Show sunrise/sunset times",
      "solar\n"
      "  Show stored location and today's solar times\n"
    },

    { "set", 2, 6, cmd_set,
      "Configure settings",
      "set date YYYY-MM-DD\n"
      "set time HH:MM:SS\n"
      "set lat  +/-DD.DDDD\n"
      "set lon  +/-DDD.DDDD\n"
      "set tz   +/-HH\n"
      "\n"
      "set door open solar  +/-MIN\n"
      "set door close solar +/-MIN\n"
      "set door open civil  +/-MIN\n"
      "set door close civil +/-MIN\n"
    },
    { "config", 0, 0, cmd_config,
      "Show configuration",
      "config\n"
      "  Show current configuration values\n"
      "  Note: changes are not committed until save\n"
    },

    { "save", 0, 0, cmd_save,
      "Commit settings",
      "save\n"
      "  Commit configuration to EEPROM and program RTC\n"
    },

    { "timeout", 1, 1, cmd_timeout,
      "Control CONFIG timeout",
      "timeout on\n"
      "timeout off\n"
      "  Enable or disable CONFIG inactivity timeout\n"
    },

    { "door", 1, 2, cmd_door,
      "Manually control door",
      "door open\n"
      "door close\n"
      "  Manually actuate the coop door\n"
    },

    { "lock", 1, 2, cmd_lock,
      "Manually control lock",
      "lock engage\n"
      "lock release\n"
      "  Manually engage or release the door lock\n"
    },

    { "exit", 0, 0, cmd_run,
      "Leave config mode",
      "exit\n"
      "  Leave CONFIG mode\n"
    },
};

#define CMD_TABLE_LEN (sizeof(cmd_table) / sizeof(cmd_table[0]))

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
