/*
 * console_time.h
 *
 * Project: Chicken Coop Controller
 * Purpose: Source file
 *
 * Notes:
 *  - Offline system
 *  - Deterministic behavior
 *  - No network dependencies
 *
 * Updated: 2025-12-29
 */

#pragma once

#include <stdint.h>
/*
 * Print full date/time in AM/PM format:
 *   YYYY-MM-DD HH:MM:SS AM|PM
 *
 * Uses console_putc().
 */
void print_datetime_ampm(int y,int mo,int d,int h,int m,int s);

//bool parse_time(const char *s, int *h, int *m, int *sec);

void print_hhmm(uint16_t minutes);

bool print_local_timedate();
