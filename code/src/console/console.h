/*
 * console.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Public interface for the interactive console / configuration system
 *
 * Features:
 *  - Command-line style configuration interface
 *  - Supports both AVR firmware (PROGMEM strings) and host simulation builds
 *  - Deterministic behavior, offline operation
 *  - Timeout protection during configuration
 *
 * Cross-platform notes:
 *  - When HOST_BUILD is defined → normal RAM strings (simulation/testing)
 *  - When HOST_BUILD is NOT defined → AVR firmware, strings in PROGMEM
 *
 * Updated: January 2026
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __AVR__
    #include <avr/pgmspace.h>
#endif

// Global flag - set to true when user wants to exit config mode
extern bool want_exit;

// Core console lifecycle
void console_init(void);
void console_poll(void);

// Timeout control - useful during long operations / wizards
void console_suspend_timeout(void);
void console_resume_timeout(void);

// -----------------------------------------------------------------------------
// Cross-platform string helpers for commands/help printing
//
// These macros automatically use the correct functions depending on build target:
//   - Firmware (AVR)   → PROGMEM + _P functions
//   - Host/simulation  → normal RAM string functions
// -----------------------------------------------------------------------------
    // Firmware (AVR): strings live in flash/PROGMEM
    #define console_strlen(s)       strlen_P(s)
    #define console_strcmp(a, b)    strcmp_P((a), (b))   // a=RAM, b=PROGMEM
    #define console_puts_str(s)     console_puts_P(s)

    // String literals go to flash via PSTR()
    #define CONSOLE_STR(s)          PSTR(s)
    // -----------------------------------------------------------------------------
// Console output helpers (declared here for completeness)
// These are usually implemented in console_io_avr.c / console_io_host.c
// -----------------------------------------------------------------------------
void console_putc(char c);
void console_puts(const char *s);       // RAM version (host uses this directly)
void console_puts_P(const char *s);     // PROGMEM version (firmware uses this)

int  console_getc(void);                // -1 if no character available

// Terminal setup (baud rate, line endings, etc.)
void console_terminal_init(void);

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
