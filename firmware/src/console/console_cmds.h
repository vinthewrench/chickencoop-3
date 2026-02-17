#pragma once

/*
 * console_cmd.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Public interface for console command dispatcher
 *
 * Notes:
 *  - No dynamic allocation
 *  - Deterministic, offline operation
 *  - Command table lives in flash (PROGMEM)
 *
 * Updated: February 2026
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dispatch a parsed console command.
 *
 * @param argc  Argument count
 * @param argv  Argument vector (tokens)
 */
void console_dispatch(int argc, char **argv);


/* ------------------------------------------------------------------
 * Command table inspection (for autocomplete + listing)
 * ------------------------------------------------------------------ */

unsigned console_cmd_count(void);
const char *console_cmd_name_at(unsigned index);

#ifdef __cplusplus
}
#endif
