/*
 * console.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Main console front-end, input handling, and command dispatch loop
 *
 * Features:
 *  - Interactive command-line interface over UART
 *  - Line editing (backspace, Ctrl-U)
 *  - Command timeout (auto-exit after inactivity)
 *  - Comment stripping (#...)
 *  - Supports both AVR firmware and host simulation builds
 *
 * Cross-platform notes:
 *  - Uses console_xxx_str() helpers from console.h
 *    → automatic PROGMEM handling on AVR
 *    → normal RAM strings on HOST_BUILD
 *
 * Memory & behavior constraints:
 *  - No dynamic allocation
 *  - Fixed-size input buffer
 *  - Deterministic, offline operation
 *
 * Updated: January 2026
 */

#include "console.h"
#include "console_io.h"
#include "console/mini_printf.h"
#include "console_time.h"
#include "rtc.h"
#include "uptime.h"
#include "config.h"

#include <string.h>


extern void console_dispatch(int argc, char **argv);
extern struct config g_cfg;


// Global exit flag - set by exit command or timeout
bool want_exit = false;

// Configuration timeout constants
#ifdef HOST_BUILD
static const uint32_t CONFIG_TIMEOUT_SEC = 60;      // shorter for development
#else
static const uint32_t CONFIG_TIMEOUT_SEC = 300;     // 5 minutes on real hardware
#endif

// Input buffer
#define MAX_LINE 64
static char buf[MAX_LINE];
static int idx = 0;

// Last activity timestamp (seconds since boot)
static uint32_t last_activity_sec = 0;

// Timeout enable/disable flag
static bool console_timeout_enabled = true;

// Forward declaration of local helpers
static void strip_comment(char *line);



/**
 * Initialize console subsystem
 * Called once at startup
 */
void console_init(void)
{
    static bool first = true;

    if (first) {
        first = false;

        console_terminal_init();

         console_puts("Chicken Coop Controller ");
         console_puts(PROJECT_VERSION);
         console_puts("\n");

     //   mini_printf("\n\nChicken Coop Controller %s\n", PROJECT_VERSION);

        // Load configuration
        bool cfg_ok = config_load(&g_cfg);
        if (!cfg_ok) {
            console_puts_str(CONSOLE_STR("WARNING: CONFIG INVALID, USING DEFAULTS\n"));
        }

        // Show current time status
        if (rtc_time_is_set()) {
            int y, mo, d, h, m, s;
            rtc_get_time(&y, &mo, &d, &h, &m, &s);
            console_puts_str(CONSOLE_STR("TIME: "));
            print_datetime_ampm(y, mo, d, h, m, s);
        } else {
            console_puts_str(CONSOLE_STR("TIME: NOT SET\n"));
            console_puts_str(CONSOLE_STR("Use: set date YYYY-MM-DD\n"));
            console_puts_str(CONSOLE_STR("     set time HH:MM:SS AM|PM\n"));
        }

        console_putc('\n');
    }

    // Reset input state
    idx = 0;
    last_activity_sec = uptime_seconds();
    console_puts_str(CONSOLE_STR("> "));
}

/**
 * Suspend automatic timeout (useful during long wizards/commands)
 */
void console_suspend_timeout(void)
{
    console_timeout_enabled = false;
}

/**
 * Resume normal timeout behavior
 */
void console_resume_timeout(void)
{
    console_timeout_enabled = true;
    last_activity_sec = uptime_seconds();  // reset timer
}

/**
 * Check if console wants to exit (timeout or 'exit' command)
 */
bool console_should_exit(void)
{
    return want_exit;
}

/**
 * Main console polling function
 * Call this frequently from main loop
 */
void console_poll(void)
{
    if (want_exit)
        return;

    // Check inactivity timeout
    if (console_timeout_enabled &&
        (uptime_seconds() - last_activity_sec) >= CONFIG_TIMEOUT_SEC) {
        console_puts_str(CONSOLE_STR("\n[CONFIG timeout]\n"));
        want_exit = true;
        return;
    }

    int c = console_getc();
    if (c < 0)
        return;

    // Any input resets timeout
    last_activity_sec = uptime_seconds();

    // Enter / Newline
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

        if (argc > 0) {
            console_dispatch(argc, argv);
        }

        idx = 0;
        console_puts_str(CONSOLE_STR("> "));
        return;
    }

    // Ctrl-U → kill current line
    if (c == 0x15) {  // ^U
        while (idx > 0) {
            console_puts_str(CONSOLE_STR("\b \b"));
            idx--;
        }
        return;
    }

    // Backspace / Delete
    if ((c == 0x08 || c == 0x7F) && idx > 0) {
        idx--;
        console_puts_str(CONSOLE_STR("\b \b"));
        return;
    }

    // Ignore non-printable characters
    if (c < 0x20 || c > 0x7E)
        return;

    // Add printable character (if buffer has space)
    if (idx < MAX_LINE - 1) {
        buf[idx++] = (char)c;
        console_putc((char)c);
    }
}

/**
 * Remove everything after # (simple comment support)
 */
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



#ifdef __AVR__
#include <avr/pgmspace.h>

void console_puts_P(const char *s)
{
    if (!s) return;
    char c;
    while ((c = pgm_read_byte(s++)) != '\0')
        console_putc(c);
}


#endif
