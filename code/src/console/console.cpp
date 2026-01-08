/*
 * console.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Console front-end and command dispatch loop
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#include "console.h"
#include "console_io.h"
#include "console/mini_printf.h"
#include "console/console_time.h"
#include "rtc.h"
#include "uptime.h"
#include "config.h"

#include <string.h>

bool want_exit = false;

#ifdef HOST_BUILD
static const uint32_t CONFIG_TIMEOUT_SEC = 60;
#else
static const uint32_t CONFIG_TIMEOUT_SEC = 300;
#endif


#define MAX_LINE 64
static char buf[MAX_LINE];
static int idx;
static uint32_t last_activity_sec;

static bool console_timeout_enabled = true;


extern void console_dispatch(int argc, char **argv);
extern struct config g_cfg;


bool console_should_exit(void)
{
    return want_exit;
}


void console_suspend_timeout(void)
{
    console_timeout_enabled = false;
}

void console_resume_timeout(void)
{
    console_timeout_enabled = true;
}


void console_init(void)
{
    static bool first = true;

    if (first) {
        first = false;

        mini_printf("Chicken Coop Controller %s\n", PROJECT_VERSION);


        console_terminal_init();

        /* LOAD CONFIG HERE */
         bool cfg_ok = config_load(&g_cfg);
          if (!cfg_ok) {
                 console_puts("WARNING: CONFIG INVALID, USING DEFAULTS\n");
             }


        if (rtc_time_is_set()) {
            int y, mo, d, h, m, s;
            rtc_get_time(&y, &mo, &d, &h, &m, &s);
            console_puts("TIME: ");
            print_datetime_ampm(y, mo, d, h, m, s);
        } else {
            console_puts("TIME: NOT SET\n");
            console_puts("Use: set date YYYY-MM-DD\n");
            console_puts("     set time HH:MM:SS AM|PM\n");
        }

        console_putc('\n');
    }

    idx = 0;
    last_activity_sec = uptime_seconds();
    console_puts("> ");
}

static void strip_comment(char *line)
{
    if (!line)
        return;

    while (*line) {
        if (*line == '#') {
            *line = '\0';
            return;
        }
        line++;
    }
}

void console_poll(void)
{
    if (want_exit)
        return;

    if (console_timeout_enabled &&
        (uptime_seconds() - last_activity_sec) >= CONFIG_TIMEOUT_SEC) {
        console_puts("\n[CONFIG timeout]\n");
        want_exit = true;
        return;
    }

    int c = console_getc();
    if (c < 0)
        return;

    last_activity_sec = uptime_seconds();

    /* --------------------------------------------------
     * Newline / Return
     * -------------------------------------------------- */
    if (c == '\n' || c == '\r') {
        console_putc('\n');

        buf[idx] = '\0';
        strip_comment(buf);

        char *argv[8];
        int argc = 0;

        char *p = strtok(buf, " ");
        while (p && argc < 8) {
            argv[argc++] = p;
            p = strtok(NULL, " ");
        }

        if (argc)
            console_dispatch(argc, argv);

        idx = 0;
        console_puts("> ");
        return;
    }

    /* --------------------------------------------------
     * Ctrl-U: kill entire line
     * -------------------------------------------------- */
    if (c == 0x15) {
        while (idx > 0) {
            console_puts("\b \b");
            idx--;
        }
        return;
    }

    /* --------------------------------------------------
     * Backspace / Delete
     * -------------------------------------------------- */
    if ((c == 0x08 || c == 0x7f) && idx > 0) {
        idx--;
        console_puts("\b \b");
        return;
    }

    /* --------------------------------------------------
     * Printable ASCII only
     * -------------------------------------------------- */
    if (c < 0x20 || c > 0x7E)
        return;

    if (idx < MAX_LINE - 1) {
        buf[idx++] = (char)c;
        console_putc((char)c);
    }
}
