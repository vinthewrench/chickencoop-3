/*
 * console_io_host.cpp
 *
 * Project: Chicken Coop Controller
 * Purpose: Host console I/O implementation
 *
 * Responsibilities:
 *  - Read console input from stdin
 *  - Write console output to stdout
 *  - Support non-blocking polling for interactive console use
 *  - Support indefinite blocking when console timeout is suspended
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *  - Host-only implementation (not used on firmware)
 *
 * Timeout behavior:
 *  - Normal mode: console_getc() performs a non-blocking poll
 *  - Suspended mode: console_getc() blocks indefinitely until input
 *  - Mode is controlled via console_suspend_timeout() /
 *    console_resume_timeout()
 *
 * Updated: 2025-12-29
 */

#include "console/console_io.h"
#include <unistd.h>
#include <sys/select.h>
#include <stdbool.h>

static bool g_console_timeout_enabled = true;

int console_getc(void)
{
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);

    int r;

    if (g_console_timeout_enabled) {
        /* non-blocking poll */
        struct timeval tv = { 0, 0 };
        r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    } else {
        /* block indefinitely */
        r = select(STDIN_FILENO + 1, &rfds, NULL, NULL, NULL);
    }

    if (r <= 0)
        return -1;

    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return c;

    return -1;
}

void console_putc(char c)
{
    write(STDOUT_FILENO, &c, 1);
}

void console_puts(const char *s)
{
    while (*s)
        console_putc(*s++);
}

void console_suspend_timeout(void)
{
    g_console_timeout_enabled = false;
}

void console_resume_timeout(void)
{
    g_console_timeout_enabled = true;
}
