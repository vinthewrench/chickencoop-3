/*
 * console.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Main console front-end, input handling, and command dispatch loop
 *
 * Features:
 *  - Interactive command-line interface over UART
 *  - Line editing (backspace, Ctrl-U)
 *  -
 * Cross-platform notes:
 *  - Uses console_xxx_str() helpers from console.h
 *    → automatic PROGMEM handling on AVR
 *    →
 *
 * Memory & behavior constraints:
 *  - No dynamic allocation
 *  - Fixed-size input buffer
 *  - Deterministic, offline operation
 *
 * Updated: January 2026
 */



 #include <avr/pgmspace.h>

#include "console.h"
#include "console_io.h"
#include "console/mini_printf.h"
#include "console_time.h"
#include "rtc.h"
#include "uptime.h"
#include "config.h"

#include <string.h>

#include "console/console_cmds.h"

// Input buffer
#define MAX_LINE 64
static char buf[MAX_LINE];
static int idx = 0;

// Forward declaration of local helpers
static void strip_comment(char *line);


/**
 * Initialize console subsystem
 * Called once at startup
 */
void console_init(void)
{

    console_terminal_init();

        console_puts("Chicken Coop Controller ");
        console_puts(PROJECT_VERSION);
        console_puts("\n");

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
        console_putc('\n');
    } else {
        console_puts_str(CONSOLE_STR("TIME: NOT SET\n"));
        console_puts_str(CONSOLE_STR("Use: set date YYYY-MM-DD\n"));
        console_puts_str(CONSOLE_STR("     set time HH:MM:SS AM|PM\n"));
    }

    console_putc('\n');

    // Reset input state
    idx = 0;
     console_puts_str(CONSOLE_STR("> "));
}


/**
 * Main console polling function
 * Call this frequently from main loop
 */
 void console_poll(void)
 {
     static bool esc_active = false;
     static bool esc_csi    = false;

     int c = console_getc();
     if (c < 0)
         return;

     // ------------------------------------------------------------
     // Swallow ANSI escape sequences (arrow keys, etc.)
     // ------------------------------------------------------------
     if (esc_active) {

         if (!esc_csi) {
             if (c == '[') {
                 esc_csi = true;
             } else {
                 esc_active = false;
             }
             return;
         }

         if (c >= 0x40 && c <= 0x7E) {
             esc_active = false;
             esc_csi    = false;
         }

         return;
     }

     if (c == 0x1B) {
         esc_active = true;
         esc_csi    = false;
         return;
     }

    // ------------------------------------------------------------
    // TAB → Autocomplete first token
    // ------------------------------------------------------------
    if (c == '\t') {

        buf[idx] = '\0';

        // Only autocomplete first word
        if (strchr(buf, ' '))
            return;

        size_t prefix_len = strlen(buf);
        if (prefix_len == 0)
            return;

        char match_buf[16];
        unsigned match_count = 0;

        unsigned count = console_cmd_count();

        for (unsigned i = 0; i < count; i++) {

            const char *name = console_cmd_name_at(i);
            if (!name)
                continue;

            char tmp[16];
            strcpy_P(tmp, (PGM_P)name);

            if (strncmp(tmp, buf, prefix_len) == 0) {

                if (match_count == 0) {
                    // Save first match
                    strncpy(match_buf, tmp, sizeof(match_buf) - 1);
                    match_buf[sizeof(match_buf) - 1] = '\0';
                }

                match_count++;
            }
        }

        if (match_count == 0)
            return;

        // --------------------------------------------------------
        // Exactly one match → complete it
        // --------------------------------------------------------
        if (match_count == 1) {

            while (idx > 0) {
                console_puts_str(CONSOLE_STR("\b \b"));
                idx--;
            }

            strncpy(buf, match_buf, MAX_LINE - 1);
            buf[MAX_LINE - 1] = '\0';
            idx = strlen(buf);

            console_puts(buf);
            return;
        }

        // --------------------------------------------------------
        // Multiple matches → list them
        // --------------------------------------------------------
        console_putc('\n');

        for (unsigned i = 0; i < count; i++) {

            const char *name = console_cmd_name_at(i);
            if (!name)
                continue;

            char tmp[16];
            strcpy_P(tmp, (PGM_P)name);

            if (strncmp(tmp, buf, prefix_len) == 0) {
                console_puts(tmp);
                console_putc('\n');
            }
        }

        console_puts_str(CONSOLE_STR("> "));
        console_puts(buf);
        return;
    }

     // ------------------------------------------------------------
     // Enter / Newline
     // ------------------------------------------------------------
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
     if (c == 0x15) {
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


void console_puts_P(const char *s)
{
    if (!s) return;
    char c;
    while ((c = pgm_read_byte(s++)) != '\0')
        console_putc(c);
}
